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

#define CIB_DUNEDAQ 1
#include <cib_data_fmt.h>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "CIBModule" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10
#define TLVL_CIB_INFO 5
#define TLVL_CIB_DEBUG 15

namespace dunedaq::cibmodules {

  CIBModule::CIBModule(const std::string& name)
              : hsilibs::HSIEventSender(name)
                , m_is_running(false)
                , m_is_configured(false)
//                , m_error_state(false)
                , m_control_ios()
//                , m_receiver_ios()
                , m_control_socket(m_control_ios)
//                , m_receiver_socket(m_receiver_ios)
                , m_thread_(std::bind(&CIBModule::do_hsi_work, this, std::placeholders::_1))
//                , m_run_packet_counter(0)
                , m_run_trigger_counter(0)
                , m_num_total_triggers(0)

                , m_num_control_messages_sent(0)
                , m_num_control_responses_received(0)
                , m_last_readout_timestamp(0)
                , m_module_instance(0)
                , m_trigger_bit(0)
                , m_receiver_ready(false)
                {
    // we can infer the instance from the name
    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Instantiating a cibmodule with argument [" << name << "]";
    register_command("conf", &CIBModule::do_configure);
    register_command("start", &CIBModule::do_start);
    register_command("stop", &CIBModule::do_stop);
    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Leaving [" << name << "]constructor.";
                }

  CIBModule::~CIBModule()
  {
    if(m_is_running)
    {
      const nlohmann::json stopobj;
      // this should also take care of closing the streaming socket
      do_stop(stopobj);
    }
    TLOG_DEBUG(TLVL_CIB_INFO) << get_name() << ": Closing the control socket " << std::endl;
    m_control_socket.close() ;
  }

  void
  CIBModule::init(const nlohmann::json& init_data)
  {
    /**
     * Typical contents of init_data
     *
    {"conn_refs":[
                  { "name":"hsievents",
                    "uid":"cib_hsievents"
                  },
                  {"name":"cib_output",
                    "uid":"cib0.cib_output_to_cib_datahandler.raw_input"
                  }
                  ]
     }
     *
     */
    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
    TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << ": Init data :  " << init_data.dump();

    // init the sender
    HSIEventSender::init(init_data);

    m_cib_hsi_data_sender = get_iom_sender<dunedaq::hsilibs::HSI_FRAME_STRUCT>(appfwk::connection_uid(init_data, "cib_output"));

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
  }

  void
  CIBModule::do_configure(const nlohmann::json& args)
  {

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering CIB do_configure()";
    TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << ": Received configuration fragment : " << args.dump();

    // this is automatically generated out of the jsonnet files in the (config) schema
    m_cfg = args.get<cibmodule::Conf>();
    nlohmann::json tmp_cfg(m_cfg);

    TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << ": Extracted configuration fragment : " << tmp_cfg.dump();

    // the configuration trigger parameters is actually representing a bitmask
    // what is defined in the configuration is the index of a map of trigger bits
    // so we have to shift the bit by the configuration parameter
    m_trigger_bit = 0x1 << m_cfg.cib_trigger_bit;
    // this is not really used right now, but keeping it for future needs
    m_module_instance = m_cfg.cib_instance;

    TLOG_DEBUG(TLVL_CIB_INFO) << get_name() << ": Instance " << m_module_instance << " assigned to trigger bit " << m_trigger_bit
        << " ( 0x" << std::hex << m_trigger_bit << std::dec << ")";


    // set local caches for the variables that are needed to set up the receiving ends
    // remember that on the server side the receiver host is necessary
    m_receiver_port = m_cfg.board_config.sockets.receiver.port;
    m_receiver_timeout = std::chrono::microseconds( m_cfg.board_config.sockets.receiver.timeout ) ;


    TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << ": Board receiver network location (from config) "
        << m_cfg.board_config.sockets.receiver.host << ':'
        << m_cfg.board_config.sockets.receiver.port
        << " (timeout = " << m_cfg.board_config.sockets.receiver.timeout << ")";

    // Initialise monitoring variables
    m_num_control_messages_sent = 0;
    m_num_control_responses_received = 0;

    //
    // network connection to cib
    //
    boost::asio::ip::tcp::resolver resolver( m_control_ios );
    // once again, these are obtained from the configuration
    // //"np04-iols-cib-01", 8991
    boost::asio::ip::tcp::resolver::query query(m_cfg.cib_host, std::to_string(m_cfg.cib_port) ) ;
    boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query) ;

    m_endpoint = iter->endpoint();

    // attempt the connection.
    try
    {
      m_control_socket.connect( m_endpoint );
    }
    catch (std::exception& e)
    {
      std::ostringstream msg("");
      msg << get_name() << "Exception caught while establishing connection to CIB : " << e.what();
      // do nothing more. Just exist
      m_is_configured.store(false);
      throw CIBCommunicationError(ERS_HERE, msg.str());
    }

    // if necessary, set the calibration stream
    // the CIB calibration stream is something a bit different
    if ( m_cfg.board_config.misc.trigger_stream_enable)
    {
      m_calibration_stream_enable = true ;
      m_calibration_dir = m_cfg.board_config.misc.trigger_stream_output ;
      m_calibration_file_interval = std::chrono::minutes(m_cfg.board_config.misc.trigger_stream_update);
    }

    // create the json string out of the config fragment
    nlohmann::json config;
    try
    {
      to_json(config, m_cfg.board_config);
    }
    catch(nlohmann::json::exception &e)
    {
      std::ostringstream msg("");
      msg << get_name() << "Caught a JSON exception converting config fragment : " << e.what();
      m_is_configured.store(false);
      throw CIBModuleError(ERS_HERE, msg.str());

    }
    catch (std::exception& e)
    {
      std::ostringstream msg("");
      msg << get_name() << "Caught STD exception while converting config fragment : " << e.what();
      // do nothing more. Just exist
      m_is_configured.store(false);
      throw CIBModuleError(ERS_HERE, msg.str());
    }

    //    TLOG() << "CONF TEST: \n" << config.dump();
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

    TLOG_DEBUG(TLVL_CIB_INFO) << get_name() << ": Sending start of run command";
    m_thread_.start_working_thread();

    // NFB: There is a potential race condition here: the socket in the working thread
    // needs to be in place before the CIB receives order to send data, or we risk having a connection
    // failure, if for some reason the CIB attempts to connect before the working thread is ready to receive.
    if ( m_calibration_stream_enable )
    {
      std::stringstream run;
      run << "run" << start_params.run;
      set_calibration_stream(run.str()) ;
    }
    int cnt = 0;
    while(!m_receiver_ready.load())
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      cnt++;
      if (cnt > 50)
      {
        // the socket didn't get ready on time
        throw CIBModuleError(ERS_HERE,"Receiver socket timed out before becoming ready.");
      }
    }
    TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << ": All ready to signal the CIB to start";

    nlohmann::json cmd;
    cmd["command"] = "start_run";
    cmd["run_number"] = start_params.run;

    if ( send_message( cmd.dump() )  )
    {
      m_is_running.store(true);
      TLOG() << get_name() << ": CIB run started successfully";
    }
    else
    {
      throw CIBCommunicationError(ERS_HERE, "Unable to start CIB run");
    }

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
  }

  void
  CIBModule::do_stop(const nlohmann::json& /*stopobj*/)
  {

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
    TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << ": Sending stop run command" << std::endl;
    if(send_message( "{\"command\":\"stop_run\"}" ) )
    {
      TLOG() << get_name() << ": CIB run stopped successfully";
      m_is_running.store( false ) ;
    }
    else
    {
      // failed to sent the message to stop the run.
      // stop the collecting thread and then throw, since that
      // attempts a cleaner exit
      m_thread_.stop_working_thread();

      throw CIBCommunicationError(ERS_HERE, "Unable to stop CIB");
    }
    //
    m_thread_.stop_working_thread();

    // -- print the counters for local info
    TLOG() << get_name() << ": CIB trigger counter summary after run [" << m_run_number << "]:\n\n"
        << "IOLS trigger counter in run : " << m_run_trigger_counter << "\n"
        << "Global IOLS trigger count   : " << m_num_total_triggers << std::endl;


    // reset counters
    m_run_trigger_counter=0;

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
    std::size_t prev_seq = 0 ;
    bool first = true;

    //connect to socket
    // should we keep everything local or under the class?
    boost::system::error_code ec;
    //boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(),m_receiver_port )

    unsigned short port = m_receiver_port;
    // check that this port is still available
    while(check_port_in_use(port))
    {
      port++;
    }
    // check if the port is different from the configured one
    if (port != m_receiver_port)
    {
      std::ostringstream msg("");
      msg << "Listener port [" << m_receiver_port << "] is in use. Relocating to port [" << port << "]";
      ers::warning(CIBMessage(ERS_HERE, msg.str()));
      m_receiver_port = port;
    }
    else
    {
      TLOG_DEBUG(TLVL_CIB_INFO) << get_name() << ": Will set up the listener on port " << port << std::endl;
    }

    boost::asio::io_service receiver_ios;
    boost::asio::ip::tcp::socket receiver_socket(receiver_ios);
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::endpoint ep( boost::asio::ip::tcp::v4(),port );
    boost::asio::ip::tcp::acceptor acceptor(io_service,ep);
    acceptor.listen(boost::asio::ip::tcp::socket::max_connections, ec);
    if (ec)
    {
      std::ostringstream msg("");
      msg << get_name() << ": CIB got an error listening on socket: :" << port << " -- reason: '" << ec << "'";
      throw CIBCommunicationError(ERS_HERE,msg.str());
      return;
    }
    else
    {
      TLOG_DEBUG(TLVL_CIB_INFO) << get_name() << ": Waiting for an incoming connection on port " << port << std::endl;
    }

    std::future<void> accepting = async( std::launch::async, [&]{ acceptor.accept(receiver_socket,ec) ; } ) ;
    if (ec)
    {
      std::stringstream msg;
      msg << "Socket opening failed:: " << ec.message();
      ers::error(CIBCommunicationError(ERS_HERE,msg.str()));
      return;
    }
    //
    m_receiver_ready.store(true);

    while ( running_flag.load() )
    {
      if ( accepting.wait_for( m_receiver_timeout ) == std::future_status::ready )
      {
        break ;
      }
//      else
//      {
//        TLOG_DEBUG(TLVL_DEBUG_MEDIUM) << "Waiting for a bit longer";
//      }
    }

    TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() <<  ": Connection received: start reading" << std::endl;

    // -- A couple of variables to help in the data parsing
    /**
     * The structure is a bit different than in the CTB
     * The TCP packet contains a single word (the trigger)
     * But other than that, everything are triggers
     */

    dunedaq::cib::daq::iols_tcp_packet_t tcp_packet;

    //boost::system::error_code receiving_error;
    bool connection_closed = false ;

    while (running_flag.load())
    {
      update_calibration_file();

      if ( ! read(receiver_socket, tcp_packet ) )
      {
        connection_closed = true ;
        break;
      }

      n_bytes = tcp_packet.header.packet_size ;
      n_words = n_bytes/sizeof(dunedaq::cib::daq::iols_trigger_t);

      if (n_words != 1)
      {
        std::ostringstream msg("");
        msg << "Received more than one IoLS trigger word at once! This should never happen. Got "
            << n_bytes << " (expected " << sizeof(dunedaq::cib::daq::iols_trigger_t) << ")";
        ers::warning(CIBMessage(ERS_HERE, msg.str()));
      }
      // the CIB only ships one word per packet....so this error should be impossible
      // check continuity of the sequence numbers
      if (first)
      {
        // first word being fetched. The sequence number should be zero
        if (tcp_packet.header.sequence_id != 0)
        {
          std::ostringstream msg("");
          msg << "Missing sequence. First word should have sequence number 0. Got " << static_cast<int>(tcp_packet.header.sequence_id );
          ers::warning(CIBMessage(ERS_HERE, msg.str()));
        }
        first = false;
      }
      else
      {
        bool failed = false;
        // in case it rolled over, compare to 255
        if (tcp_packet.header.sequence_id == 0)
        {
          if (prev_seq != 255)
          {
            failed = true;
          }
        }
        else
        {
          if (tcp_packet.header.sequence_id != (prev_seq+1))
          {
            failed = true;
          }
        }
        if (failed)
        {
          std::ostringstream msg("");
          msg << "Skipped CIB word sequence. Prev word " << prev_seq << " current word " << static_cast<int>(tcp_packet.header.sequence_id );
          ers::warning(CIBMessage(ERS_HERE, msg.str()));
        }
      }
      prev_seq = tcp_packet.header.sequence_id;

      update_buffer_counts(n_words);


      if ( m_calibration_stream_enable )
      {
        m_calibration_file.write( reinterpret_cast<const char*>( & tcp_packet.word ), sizeof(tcp_packet.word) ) ; // NOLINT
        m_calibration_file.flush() ;
      }

      TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << "Received IoLS trigger word!";
      ++m_num_total_triggers;
      ++m_run_trigger_counter;
      m_last_readout_timestamp = tcp_packet.word.timestamp;

      // we do not need to know anything else
      // ideally, one could add other information such as the direction
      // this should be coming packed in the trigger word
      // note, however, that to reconstruct the trace direction we also would need the source position
      // and that we cannot afford to send, so we can just make it up out of the  IoLS system
      //
      // Send HSI data to a DLH
      std::array<uint32_t, 7> hsi_struct;
      hsi_struct[0] = (0x1 << 26) | (0x1 << 6) | 0x1; // DAQHeader, frame version: 1, det id: 1, link for low level 0, link for high level 1, leave slot and crate as 0
      hsi_struct[1] = tcp_packet.word.timestamp & 0xFFFFFFFF;       // ts low
      hsi_struct[2] = tcp_packet.word.timestamp >> 32;            // ts high
      // we could use these 2 sets of 32 bits to identify the direction
      // TODO Nuno Barros Apr-02-2024: Propose to change this to include additional information
      // these 64 bits could be used to define a direction
      /**
       * A note about the 5th entry
       * The trigger bit is actually mapped into a single bit, that is then remapped back
       * into an index
       */
      hsi_struct[3] = 0x0;                      // lower 32b
      hsi_struct[4] = 0x0;                      // upper 32b
      hsi_struct[5] = m_trigger_bit;            // trigger_map;
      hsi_struct[6] = m_run_trigger_counter;    // m_generated_counter;

      TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << ": Formed HSI_FRAME_STRUCT for hlt "
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

      // TODO Nuno Barros Apr-02-2024 : properly fill device id
      // still need to figure this one out.
      dfmessages::HSIEvent event = dfmessages::HSIEvent(0x1,
                                                        m_trigger_bit,
                                                        tcp_packet.word.timestamp,
                                                        m_run_trigger_counter, m_run_number);
      // FIXME: Could we override this class to pass on extra information to the candidate maker?
      send_hsi_event(event);

      if ( connection_closed )
      {
        break ;
      }
    }

    boost::system::error_code closing_error;
    // NFB: This was intended for a case when the CIB could have troubles fetching data and therefore could
    //      pass an error state word. That functionality has been removed and therefore this error_state is
    //      now obsolete
    //    if ( m_error_state.load() )
    //    {
    //
    //      receiver_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, closing_error);
    //
    //      if ( closing_error )
    //      {
    //        std::stringstream msg;
    //        msg << "Error in shutdown " << closing_error.message();
    //        ers::error(CIBCommunicationError(ERS_HERE,msg.str())) ;
    //      }
    //    }

    receiver_socket.close(closing_error) ;

    if ( closing_error )
    {
      std::stringstream msg;
      msg << "Socket closing failed:: " << closing_error.message();
      ers::error(CIBCommunicationError(ERS_HERE,msg.str()));
    }

    m_receiver_ready.store(false);

    TLOG_DEBUG(TLVL_CIB_INFO) << get_name() << ": End of do_work loop: stop receiving data from the CIB";

    TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
  }

  template<typename T>
  bool CIBModule::read(boost::asio::ip::tcp::socket &socket, T &obj)
  {

    boost::system::error_code receiving_error;
    boost::asio::read( socket,
                       boost::asio::buffer( &obj, sizeof(T) ),
                       receiving_error ) ;

    if ( ! receiving_error )
    {
      return true ;
    }

    if ( receiving_error == boost::asio::error::eof)
    {
      std::string error_message = "Socket closed: " + receiving_error.message();
      ers::error(CIBCommunicationError(ERS_HERE, error_message));
      return false ;
    }

    if ( receiving_error )
    {
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
    struct tm local_tm;
    struct tm * timeinfo = localtime_r( & rawtime , &local_tm) ;
    strftime( file_name, sizeof(file_name), "%F_%H.%M.%S.calib", timeinfo );
    std::string global_name = m_calibration_dir + m_calibration_prefix + file_name ;
    m_calibration_file.open( global_name, std::ofstream::binary ) ;
    m_last_calibration_file_update = std::chrono::steady_clock::now();
    // _calibration_file.setf ( std::ios::hex, std::ios::basefield );
    // _calibration_file.unsetf ( std::ios::showbase );
    TLOG_DEBUG(TLVL_CIB_INFO) << get_name() << ": New Calibration Stream file: " << global_name << std::endl ;
  }

  void CIBModule::update_calibration_file()
  {

    if ( ! m_calibration_stream_enable )
    {
      return ;
    }

    std::chrono::steady_clock::time_point check_point = std::chrono::steady_clock::now();

    if ( check_point - m_last_calibration_file_update < m_calibration_file_interval )
    {
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

  void CIBModule::send_config( const std::string & config ) {

//    if ( m_is_configured.load() )
//    {
//      TLOG() << get_name() << ": Resetting before configuring" << std::endl;
//      send_reset();
//    }

    TLOG_DEBUG(TLVL_CIB_INFO) << get_name() << ": Sending config" << std::endl;

    // structure the message to have a common management structure
    //json receiver = doc.at("ctb").at("sockets").at("receiver");

    nlohmann::json conf;
    conf["command"] = "config";
    conf["config"] = nlohmann::json::parse(config);

    TLOG_DEBUG(TLVL_CIB_DEBUG) << get_name() << ": Shipped config : " << conf.dump() << std::endl;

    if ( send_message( conf.dump() ) )
    {
      m_is_configured.store(true) ;
    }
    else
    {
      throw CIBCommunicationError(ERS_HERE, "Unable to configure CIB");
    }
  }

//  void CIBModule::send_reset()
//  {
//    // actually, we do not want to do this to the CIB
//    // the reset should go through the slow control
//
//    TLOG_DEBUG(1) << get_name() << ": NOT Sending a reset" << std::endl;
//
//    return;
//
//  }

  bool CIBModule::send_message( const std::string & msg )
  {

    //add error options
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
        TLOG() << "Message from the CIB : " << messages[i]["message"].dump();
      }
      else
      {
        std::stringstream blob;
        blob << messages[i] ;
        TLOG() << get_name() << ": Unformatted feedback from the board: " << blob.str();
      }
    }
    return ret;
  }

  void
  CIBModule::update_buffer_counts(uint new_count) // NOLINT(build/unsigned)
  {
    std::unique_lock mon_data_lock(m_buffer_counts_mutex);
    if (m_buffer_counts.size() > 1000)
    {
      m_buffer_counts.pop_front();
    }
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
    module_info.cib_num_triggers_received = m_num_total_triggers;

    module_info.last_readout_timestamp = m_last_readout_timestamp.load();
    // -- need to define these counters (and set the code to update them
    module_info.sent_hsi_events_counter = m_sent_counter.load();
    module_info.failed_to_send_hsi_events_counter = m_failed_to_send_counter.load();
    module_info.last_sent_timestamp = m_last_sent_timestamp.load();
    module_info.average_buffer_occupancy = read_average_buffer_counts();

    ci.add(module_info);
  }

  bool CIBModule::check_port_in_use(unsigned short port)
  {
    using namespace boost::asio;
    using ip::tcp;

    io_service svc;
    tcp::acceptor a(svc);

    boost::system::error_code ec;
    a.open(tcp::v4(), ec) || a.bind({ tcp::v4(), port }, ec);
    a.close();
    return ec == error::address_in_use;

  }




} // namespace dunedaq::cibmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::cibmodules::CIBModule)
