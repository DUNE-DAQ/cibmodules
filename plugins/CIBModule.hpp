/**
 * @file CIBModule.hpp
 *
 * Developer(s) of this DAQModule have yet to replace this line with a brief description of the DAQModule.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef CIBMODULES_PLUGINS_CIBMODULE_HPP_
#define CIBMODULES_PLUGINS_CIBMODULE_HPP_

#include "appfwk/DAQModule.hpp"
#include "iomanager/Receiver.hpp"
#include "iomanager/Sender.hpp"
#include "utilities/WorkerThread.hpp"

#include "hsilibs/HSIEventSender.hpp"

#include <ers/Issue.hpp>

#include "cibmodules/cibmodule/Nljs.hpp"
#include "cibmodules/cibmoduleinfo/InfoNljs.hpp"

#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <shared_mutex>

#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <atomic>
#include <limits>
#include <string>

namespace dunedaq::cibmodules {

  class CIBModule : public dunedaq::hsilibs::HSIEventSender
  {
  public:
    /**
     * @brief CTBModule Constructor
     * @param name Instance name for this CTBModule instance
     */
    explicit CIBModule(const std::string& name);

    //void init(const nlohmann::json& iniobj) override;
    void init(const nlohmann::json& iniobj) override;

    /**
     * Disallow copy and move constructors and assignments
     */
    CIBModule(const CIBModule&) = delete;
    CIBModule& operator=(const CIBModule&) = delete;
    CIBModule(CIBModule&&) = delete;
    CIBModule& operator=(CIBModule&&) = delete;

    ~CIBModule();

    bool ErrorState() const { return m_error_state.load() ; }
    void get_info(opmonlib::InfoCollector& ci, int level) override;

  private:

    // control variables
    std::atomic<bool> m_is_running;
    std::atomic<bool> m_is_configured;

    /*const */unsigned int m_receiver_port;
    std::chrono::microseconds m_receiver_timeout;


    //  std::chrono::microseconds m_timeout;



    std::atomic<bool> m_error_state;

    boost::asio::io_service m_control_ios;
    boost::asio::io_service m_receiver_ios;
    boost::asio::ip::tcp::socket m_control_socket;
    boost::asio::ip::tcp::socket m_receiver_socket;
    boost::asio::ip::tcp::endpoint m_endpoint;

    std::shared_ptr<dunedaq::hsilibs::HSIEventSender::raw_sender_ct> m_cib_hsi_data_sender;
    //  std::shared_ptr<dunedaq::hsilibs::HSIEventSender::raw_sender_ct> m_hlt_hsi_data_sender;


    // Commands
    void do_configure(const nlohmann::json& obj);
    void do_start(const nlohmann::json& startobj);
    void do_stop(const nlohmann::json& obj);
    // NFB: what's this for?
    void do_scrap(const nlohmann::json& /*obj*/) { }

    // the CIB does not need reset, since the DAQ operation is
    // decoupled from the instrumentation operation
    void send_reset() ;
    void send_config(const std::string & config);
    bool send_message(const std::string & msg);

    // Configuration
    dunedaq::cibmodules::cibmodule::Conf m_cfg;
    std::atomic<daqdataformats::run_number_t> m_run_number;

    // Threading
    dunedaq::utilities::WorkerThread m_thread_;
    void do_hsi_work(std::atomic<bool>&);

    template<typename T>
    bool read(T &obj);

    //
    //
    // members related to calibration stream
    //
    // this is a standalone output parallel to the DAQ
    void update_calibration_file();
    void init_calibration_file();
    bool set_calibration_stream( const std::string &prefix = "" );

    // -- calibration stream stuff
    bool m_calibration_stream_enable = false;
    std::string m_calibration_dir = "";
    std::string m_calibration_prefix = "";
    std::chrono::minutes m_calibration_file_interval;
    std::ofstream m_calibration_file;
    std::chrono::steady_clock::time_point m_last_calibration_file_update;

    // counters per run
    //  std::atomic<unsigned long> m_run_gool_part_counter = 0;
    std::atomic<unsigned long> m_run_packet_counter = 0;
    std::atomic<unsigned long> m_run_trigger_counter = 0;
    // we no longer have timestamp words
    //  std::atomic<unsigned long> m_run_timestamp_counter = 0;
    // overall counters
    //  std::atomic<unsigned int> m_num_TS_words;
    std::atomic<unsigned int> m_num_total_triggers;


    //
    //
    // monitoring data/information
    //
    //
    std::deque<uint> m_buffer_counts; // NOLINT(build/unsigned)
    std::shared_mutex m_buffer_counts_mutex;
    void update_buffer_counts(uint new_count); // NOLINT(build/unsigned)
    double read_average_buffer_counts();

    std::atomic<int>      m_num_control_messages_sent;
    std::atomic<int>      m_num_control_responses_received;
    std::atomic<uint64_t> m_last_readout_timestamp; // NOLINT(build/unsigned)
  };

} // namespace dunedaq::cibmodules

#endif // CIBMODULES_PLUGINS_CIBMODULE_HPP_
