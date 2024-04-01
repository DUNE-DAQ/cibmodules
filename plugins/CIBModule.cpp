/**
 * @file CIBModule.cpp
 *
 * Implementations of CIBModule's functions
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "CIBModule.hpp"
#include "CIBModuleIssues.hpp"
#include "appfwk/DAQModuleHelper.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"
#include "cibmodules/cibmodule/Nljs.hpp"
#include "cibmodules/cibmoduleinfo/InfoNljs.hpp"
#include "rcif/cmd/Nljs.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "CIBModule" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10
#define TLVL_CTB_MODULE 15

namespace dunedaq::cibmodules {

  CIBModule::CIBModule(const std::string& name)
              : hsilibs::HSIEventSender(name)
                , m_is_running(false)
                , m_is_configured(false)
                , m_num_TS_words(0)
                , m_num_TR_words(0)
                , m_error_state(false)
                , m_control_ios()
                , m_receiver_ios()
                , m_control_socket(m_control_ios)
                , m_receiver_socket(m_receiver_ios)
                , m_thread_(std::bind(&CIBModule::do_hsi_work, this, std::placeholders::_1))
                , m_run_trigger_counter(0)
                , m_num_control_messages_sent(0)
                , m_num_control_responses_received(0)
                , m_last_readout_timestamp(0)
                {
    register_command("conf", &CIBModule::do_configure);
    register_command("start", &CIBModule::do_start);
    register_command("stop", &CIBModule::do_stop);
                }

  CIBModule::~CIBModule()
  {
    if(m_is_running)
    {
      const nlohmann::json stopobj;
      // this should also take care of closing the streaming socket
      do_stop(stopobj);
    }
    m_control_socket.close() ;
  }

  void
  CIBModule::init(const nlohmann::json& init_data)
  {
    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
    HSIEventSender::init(init_data);
    //FIXME: Do we need to set up something specifically to set up this uuid?
    m_cib_hsi_data_sender = get_iom_sender<dunedaq::hsilibs::HSI_FRAME_STRUCT>(appfwk::connection_uid(init_data, "cib_output"));

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
  }

  void
  CIBModule::do_configure(const nlohmann::json& args)
  {

    TLOG_DEBUG(0) << get_name() << ": Configuring CIB";

    // this is automatically generated out of the jsonnet files in the (config) schema
    m_cfg = args.get<cibmodule::Conf>();
    // set local caches for the variables that are needed to set up the receiving ends
    // this should be localhost
    //m_receiver_host = m_cfg.board_config.cib.sockets.receiver.host;
    m_receiver_port = m_cfg.board_config.cib.sockets.receiver.port;
    m_receiver_timeout = std::chrono::microseconds( m_cfg.receiver_connection_timeout ) ;

    TLOG_DEBUG(0) << get_name() << ": Board receiver network location "
        << m_cfg.board_config.cib.sockets.receiver.host << ':'
        << m_cfg.board_config.cib.sockets.receiver.port << std::endl;

    // Initialise monitoring variables
    m_num_control_messages_sent = 0;
    m_num_control_responses_received = 0;

    // network connection to cib hardware control
    boost::asio::ip::tcp::resolver resolver( m_control_ios );
    // once again, these are obtained from the configuration
    boost::asio::ip::tcp::resolver::query query(m_cfg.cib_control_host,
                                                std::to_string(m_cfg.cib_control_port) ) ; //"np04-ctb-1", 8991
    boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query) ;

    m_endpoint = iter->endpoint();

    // attempt the connection.
    // FIXME: what to do if that fails?
    try
    {
      m_control_socket.connect( m_endpoint );
    }
    catch (std::exception& e)
    {
      TLOG() << "Exeption caught while establishing connection to CIB : " << e.what();
      // do nothing more. Just exist
      m_is_configured.store(false);
      return;
    }

    // if necessary, set the calibration stream
    // the CIB calibration stream is something a bit different
    if ( m_cfg.board_config.cib.misc.trigger_stream_enable)
    {
      m_calibration_stream_enable = true ;
      m_calibration_dir = m_cfg.board_config.cib.misc.trigger_stream_output ;
      m_calibration_file_interval = std::chrono::minutes(m_cfg.board_config.cib.misc.trigger_stream_update);
    }

    // create the json string out of the config fragment
    nlohmann::json config;
    to_json(config, m_cfg.board_config);
    TLOG() << "CONF TEST: " << config.dump();

    // FIXME: Actually would prefer to use protobufs, but this is also acceptable
    send_config(config.dump());
  }

  void
  CIBModule::do_start(const nlohmann::json& startobj)
  {

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

    // actually, the first thing to check is whether the CIB has been configured
    // if not, this won't work
    if (!m_is_configured.load())
    {
      throw CIBWrongState(ERS_HERE,"CIB has not been successfully configured.");
    }

    auto start_params = startobj.get<rcif::cmd::StartParams>();
    // this is actually part of the run command sent to the CIB
    m_run_number.store(start_params.run);

    TLOG_DEBUG(0) << get_name() << ": Sending start of run command";
    m_thread_.start_working_thread();

    if ( m_calibration_stream_enable )
    {
      std::stringstream run;
      run << "run" << start_params.run;
      set_calibration_stream(run.str()) ;
    }
    nlohmann::json cmd;
    cmd["command"] = "start_run";
    cmd["run_number"] = start_params.run;
    // FIXME: Convert this to protobuf
    if ( send_message( cmd.dump() )  )
    {
      m_is_running.store(true);
      TLOG_DEBUG(1) << get_name() << ": successfully started";
    }
    else
    {
      throw CIBCommunicationError(ERS_HERE, "Unable to start rim CIB");
    }

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
  }

  void
  CIBModule::do_stop(const nlohmann::json& /*stopobj*/)
  {

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

    TLOG_DEBUG(0) << get_name() << ": Sending stop run command" << std::endl;
    // FIXME: Convert this to protobuf
    if(send_message( "{\"command\":\"stop_run\"}" ) )
    {
      TLOG_DEBUG(1) << get_name() << ": successfully stopped";
      m_is_running.store( false ) ;
    }
    else
    {
      throw CIBCommunicationError(ERS_HERE, "Unable to stop CIB");
    }
    //
    //store_run_trigger_counters( m_run_number ) ;
    m_thread_.stop_working_thread();


    // reset counters

    m_run_trigger_counter=0;
    m_run_packet_counter=0;

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
  }
  // this method is completely new
  // in fact, it is where most of the work is really done
  void
  CIBModule::do_hsi_work(std::atomic<bool>& running_flag)
  {
    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";

    std::size_t n_bytes = 0 ;
    std::size_t n_words = 0 ;

    const size_t header_size = sizeof( content::tcp_header_t ) ;
    const size_t word_size = content::word::word_t::size_bytes ;

    TLOG_DEBUG(TLVL_CTB_MODULE) << get_name() <<  ": Header size: " << header_size << std::endl
        << "Word size: " << word_size << std::endl;

    //connect to socket
    boost::asio::ip::tcp::acceptor acceptor(m_receiver_ios, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), m_receiver_port ) );
    TLOG_DEBUG(0) << get_name() << ": Waiting for an incoming connection on port " << m_receiver_port << std::endl;

    std::future<void> accepting = async( std::launch::async, [&]{ acceptor.accept(m_receiver_socket) ; } ) ;

    while ( running_flag.load() )
    {
      if ( accepting.wait_for( m_timeout ) == std::future_status::ready )
      {
        break ;
      }
    }

    TLOG_DEBUG(0) << get_name() <<  ": Connection received: start reading" << std::endl;

    // -- A couple of variables to help in the data parsing
    /**
     * The structure is a bit different than in the CTB
     * The TCP packet is still wrapped in a header, and there is a timestamp word
     * marking the last packet
     * But other than that, everything are triggers
     */
    content::tcp_header_t head ;
    head.packet_size = 0;
    content::word::word_t temp_word ;
    boost::system::error_code receiving_error;
    bool connection_closed = false ;
    uint64_t ch_stat_beam, ch_stat_crt, ch_stat_pds;
    uint64_t llt_payload, channel_payload;
    uint64_t prev_timestamp = 0;
    std::pair<uint64_t,uint64_t> prev_channel, prev_prev_channel, prev_llt, prev_prev_llt; // pair<timestamp, trigger_payload>

    while (running_flag.load())
    {

      update_calibration_file();

      if ( ! read( head ) ) {
        connection_closed = true ;
        break;
      }

      n_bytes = head.packet_size ;
      // extract n_words

      n_words = n_bytes / word_size ;
      // read n words as requested from the header

      update_buffer_counts(n_words);

      for ( unsigned int i = 0 ; i < n_words ; ++i )
      {
        //read a word
        if ( ! read( temp_word ) )
        {
          connection_closed = true ;
          break ;
        }
        // put it in the calibration stream
        if ( m_calibration_stream_enable )
        {
          m_calibration_file.write( reinterpret_cast<const char*>( & temp_word ), word_size ) ;
          m_calibration_file.flush() ;
        }          // word printing in calibration stream

        //check if it is a TS word and increment the counter
        if ( temp_word.word_type == content::word::t_ts )
        {
          m_num_TS_words++ ;
          TLOG_DEBUG(9) << "Received timestamp word! TS: "+temp_word.timestamp;
          prev_timestamp = temp_word.timestamp;
        }
        // FIXME: Should we reintroduce these
        // not for now. Use slow control to monitor the internal buffers
        else if (  temp_word.word_type == content::word::t_fback  )
        {
          m_error_state.store( true ) ;
          content::word::feedback_t * feedback = reinterpret_cast<content::word::feedback_t*>( & temp_word ) ;
          TLOG_DEBUG(7) << "Received feedback word!";

          TLOG_DEBUG(8) << get_name() << ": Feedback word: " << std::endl
              << std::hex
              << " \t Type -> " << feedback -> word_type << std::endl
              << " \t TS -> " << feedback -> timestamp << std::endl
              << " \t Code -> " << feedback -> code << std::endl
              << " \t Source -> " << feedback -> source << std::endl
              << " \t Padding -> " << feedback -> padding << std::dec << std::endl ;
        }
        else if (temp_word.word_type == content::word::t_trigger)
        {
          TLOG_DEBUG(3) << "Received IoLS trigger word!";
          ++m_num_TR_words;
          content::word::trigger_t * trigger_word = reinterpret_cast<content::word::trigger_t*>( & temp_word ) ;

          m_last_readout_timestamp = trigger_word->timestamp;
          // we do not need to knwo anything else
          // ideally, one could add other information such as the direction
          // this should be coming packed in the trigger word
          // note, however, that to reconstruct the trace direction we also would need the source position
          // and that we cannot afford to send, so we can just make it up out of the
          // IoLS system
          //
          // Send HSI data to a DLH
          std::array<uint32_t, 7> hsi_struct;
          hsi_struct[0] = (0x1 << 26) | (0x1 << 6) | 0x1; // DAQHeader, frame version: 1, det id: 1, link for low level 0, link for high level 1, leave slot and crate as 0
          hsi_struct[1] = trigger_word->timestamp & 0xFFFFFF;       // ts low
          hsi_struct[2] = trigger_word->timestamp >> 32; // ts high
          // we could use these 2 sets of 32 bits to identify the direction
          // TODO: Propose to change this to include additional information
          // these 64 bits could be used to define a direction
          hsi_struct[3] = 0x0;                      // lower 32b
          hsi_struct[4] = 0x0;                      // upper 32b
          hsi_struct[5] = trigger_word->trigger_word;    // trigger_map;
          hsi_struct[6] = m_run_trigger_counter;         // m_generated_counter;

          TLOG_DEBUG(4) << get_name() << ": Formed HSI_FRAME_STRUCT for hlt "
              << std::hex
              << "0x"   << hsi_struct[0]
                                      << ", 0x" << hsi_struct[1]
                                                              << ", 0x" << hsi_struct[2]
                                                                                      << ", 0x" << hsi_struct[3]
                                                                                                              << ", 0x" << hsi_struct[4]
                                                                                                                                      << ", 0x" << hsi_struct[5]
                                                                                                                                                              << ", 0x" << hsi_struct[6]
                                                                                                                                                                                      << "\n";

          send_raw_hsi_data(hsi_struct, m_cib_hsi_data_sender.get());

          // TODO properly fill device id
          dfmessages::HSIEvent event = dfmessages::HSIEvent(0x1,
                                                            trigger_word->trigger_word,
                                                            trigger_word->timestamp,
                                                            m_run_trigger_counter, m_run_number);
          send_hsi_event(event);
        }
      } // n_words loop

      if ( connection_closed )
      {
        break ;
      }
    }

    boost::system::error_code closing_error;

    if ( m_error_state.load() )
    {

      m_receiver_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, closing_error);

      if ( closing_error )
      {
        std::stringstream msg;
        msg << "Error in shutdown " << closing_error.message();
        ers::error(CIBCommunicationError(ERS_HERE,msg.str())) ;
      }
    }

    m_receiver_socket.close(closing_error) ;

    if ( closing_error )
    {
      std::stringstream msg;
      msg << "Socket closing failed:: " << closing_error.message();
      ers::error(CIBCommunicationError(ERS_HERE,msg.str()));
    }


    TLOG_DEBUG(TLVL_CTB_MODULE) << get_name() << ": End of do_work loop: stop receiving data from the CIB";

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
  }

  template<typename T>
  bool CIBModule::read( T &obj) {

    boost::system::error_code receiving_error;
    boost::asio::read( m_receiver_socket, boost::asio::buffer( &obj, sizeof(T) ), receiving_error ) ;

    if ( ! receiving_error ) {
      return true ;
    }

    if ( receiving_error == boost::asio::error::eof) {
      std::string error_message = "Socket closed: " + receiving_error.message();
      ers::error(CIBCommunicationError(ERS_HERE, error_message));
      return false ;
    }

    if ( receiving_error ) {
      std::string error_message = "Read failure: " + receiving_error.message();
      ers::error(CIBCommunicationError(ERS_HERE, error_message));
      return false ;
    }

    return true ;
  }

  void CIBModule::init_calibration_file()
  {

    if ( ! m_calibration_stream_enable )
    {
      return ;
    }
    char file_name[200] = "" ;
    time_t rawtime;
    time( & rawtime ) ;
    struct tm * timeinfo = localtime( & rawtime ) ;
    strftime( file_name, sizeof(file_name), "%F_%H.%M.%S.calib", timeinfo );
    std::string global_name = m_calibration_dir + m_calibration_prefix + file_name ;
    m_calibration_file.open( global_name, std::ofstream::binary ) ;
    m_last_calibration_file_update = std::chrono::steady_clock::now();
    // _calibration_file.setf ( std::ios::hex, std::ios::basefield );
    // _calibration_file.unsetf ( std::ios::showbase );
    TLOG_DEBUG(0) << get_name() << ": New Calibration Stream file: " << global_name << std::endl ;

  }

  void CIBModule::update_calibration_file()
  {

    if ( ! m_calibration_stream_enable )
    {
      return ;
    }

    std::chrono::steady_clock::time_point check_point = std::chrono::steady_clock::now();

    if ( check_point - m_last_calibration_file_update < m_calibration_file_interval ) {
      return ;
    }

    m_calibration_file.close() ;
    init_calibration_file() ;

  }

  bool CIBModule::set_calibration_stream( const std::string & prefix )
  {

    if ( m_calibration_dir.back() != '/' )
    {
      m_calibration_dir += '/' ;
    }
    m_calibration_prefix = prefix ;
    if ( prefix.size() > 0 )
    {
      m_calibration_prefix += '_' ;
    }
    // possibly we could check here if the directory is valid and  writable before assuming the calibration stream is valid
    return true ;
  }
  // do we actually need this summary?
  //  bool CIBModule::store_run_trigger_counters( unsigned int run_number, const std::string & prefix) const {
  //
  //    if ( ! m_has_run_trigger_report )
  //    {
  //      return false ;
  //    }
  //
  //    std::stringstream out_name ;
  //    out_name << m_run_trigger_dir << prefix << "run_" << run_number << "_triggers.txt";
  //    std::ofstream out( out_name.str() ) ;
  //    out << "Good Part\t " << m_run_gool_part_counter << std::endl
  //        << "Total HLT\t " << m_run_HLT_counter << std::endl ;
  //
  //    for ( unsigned int i = 0; i < m_metric_HLT_names.size() ; ++i )
  //    {
  //      out << "HLT " << i << " \t " << m_run_HLT_counters[i] << std::endl ;
  //    }
  //
  //    return true;
  //
  //  }

  void CIBModule::send_config( const std::string & config ) {

    if ( m_is_configured.load() )
    {
      TLOG_DEBUG(1) << get_name() << ": Resetting before configuring" << std::endl;
      send_reset();
    }

    TLOG_DEBUG(1) << get_name() << ": Sending config" << std::endl;

    // structure the message to have a common management structure
    //json receiver = doc.at("ctb").at("sockets").at("receiver");

    nlohmann::json conf;
    conf["command"] = "config";
    conf["config"] = config;

    if ( send_message( conf.dump() ) )
    {
      m_is_configured.store(true) ;
    }
    else
    {
      throw CIBCommunicationError(ERS_HERE, "Unable to configure CIB");
    }
  }

  void CIBModule::send_reset()
  {
    // actually, we do not want to do this to the CIB
    // the reset should go through the slow control

    TLOG_DEBUG(1) << get_name() << ": NOT Sending a reset" << std::endl;

    return;

    //    // actually, we do not want to do this to the CIB
    //    // the reset should go through the slow control
    //    if(send_message( "{\"command\":\"HardReset\"}" ))
    //    {
    //
    //      m_is_running.store(false);
    //      m_is_configured.store(false);
    //
    //    }
    //    else{
    //      ers::error(CTBCommunicationError(ERS_HERE, "Unable to reset CTB"));
    //    }

  }

  bool CIBModule::send_message( const std::string & msg )
  {

    //add error options
    //FIXME: Migrate this to protobuf
    boost::system::error_code error;
    TLOG_DEBUG(1) << get_name() << ": Sending message: " << msg;

    m_num_control_messages_sent++;

    boost::asio::write( m_control_socket, boost::asio::buffer( msg ), error ) ;
    boost::array<char, 1024> reply_buf{" "} ;
    m_control_socket.read_some( boost::asio::buffer(reply_buf ), error);
    std::stringstream raw_answer( std::string(reply_buf .begin(), reply_buf .end() ) ) ;
    TLOG_DEBUG(1) << get_name() << ": Unformatted answer: " << raw_answer.str();

    nlohmann::json answer ;
    raw_answer >> answer ;
    nlohmann::json & messages = answer["feedback"] ;
    TLOG_DEBUG(1) << get_name() << ": Received messages: " << messages.size();

    bool ret = true ;
    for (nlohmann::json::size_type i = 0; i != messages.size(); ++i )
    {

      m_num_control_responses_received++;

      std::string type = messages[i]["type"].dump() ;
      if ( type.find("error") != std::string::npos || type.find("Error") != std::string::npos || type.find("ERROR") != std::string::npos )
      {
        ers::error(CIBMessage(ERS_HERE, messages[i]["message"].dump()));
        ret = false ;
      }
      else if ( type.find("warning") != std::string::npos || type.find("Warning") != std::string::npos || type.find("WARNING") != std::string::npos )
      {
        ers::warning(CIBMessage(ERS_HERE, messages[i]["message"].dump()));
      }
      else if ( type.find("info") != std::string::npos || type.find("Info") != std::string::npos || type.find("INFO") != std::string::npos)
      {
        TLOG() << "Message from the board: " << messages[i]["message"].dump();
      }
      else
      {
        std::stringstream blob;
        blob << messages[i] ;
        TLOG() << get_name() << ": Unformatted from the board: " << blob.str();
      }
    }

    return ret;

  }

  void
  CIBModule::update_buffer_counts(uint new_count) // NOLINT(build/unsigned)
  {
    std::unique_lock mon_data_lock(m_buffer_counts_mutex);
    if (m_buffer_counts.size() > 1000)
      m_buffer_counts.pop_front();
    m_buffer_counts.push_back(new_count);
  }

  double
  CIBModule::read_average_buffer_counts()
  {
    std::unique_lock mon_data_lock(m_buffer_counts_mutex);

    double total_counts;
    uint32_t number_of_counts; // NOLINT(build/unsigned)

    total_counts = 0;
    number_of_counts = m_buffer_counts.size();

    if (number_of_counts) {
      for (uint i = 0; i < number_of_counts; ++i) { // NOLINT(build/unsigned)
        total_counts = total_counts + m_buffer_counts.at(i);
      }
      return total_counts / number_of_counts;
    } else {
      return 0;
    }
  }

  void CIBModule::get_info(opmonlib::InfoCollector& ci, int /*level*/)
  {
    dunedaq::cibmodules::cibmoduleinfo::CIBModuleInfo module_info;

    module_info.num_control_messages_sent = m_num_control_messages_sent.load();
    module_info.num_control_responses_received = m_num_control_responses_received.load();
    module_info.cib_hardware_run_status = m_is_running;
    module_info.cib_hardware_configuration_status = m_is_configured;
    module_info.num_ts_words_received = m_num_TS_words;

    module_info.last_readout_timestamp = m_last_readout_timestamp.load();
    // -- need to define these counters (and set the code to update them
    module_info.sent_hsi_events_counter = m_sent_counter.load();
    module_info.failed_to_send_hsi_events_counter = m_failed_to_send_counter.load();
    module_info.last_sent_timestamp = m_last_sent_timestamp.load();
    module_info.average_buffer_occupancy = read_average_buffer_counts();

    ci.add(module_info);
  }

} // namespace dunedaq::cibmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::cibmodules::CIBModule)
