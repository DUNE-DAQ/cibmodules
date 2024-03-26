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
      , m_n_TS_words(0)
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
CIBModule::init(const data_t& /* structured args */)
{}

void
CIBModule::get_info(opmonlib::InfoCollector& ci, int /* level */)
{
  cibmoduleinfo::Info info;
  info.total_amount = m_total_amount;
  info.amount_since_last_get_info_call = m_amount_since_last_get_info_call.exchange(0);

  ci.add(info);
}

void
CIBModule::do_conf(const data_t& conf_as_json)
{
  auto conf_as_cpp = conf_as_json.get<cibmodule::Conf>();
  m_some_configured_value = conf_as_cpp.some_configured_value;
}

} // namespace dunedaq::cibmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::cibmodules::CIBModule)
