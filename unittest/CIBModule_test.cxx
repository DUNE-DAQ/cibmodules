/**
 * @file CIBModule_test.cxx
 *
 * Unit tests targeting basic CIBModule behavior.
 */

#define BOOST_TEST_MODULE CIBModule_test // NOLINT

#include "appfwk/DAQModule.hpp"
#include "appmodel/CIBModule.hpp"
#include "cibmodules/opmon/CIBModule.pb.h"
#include "hsilibs/HSIEventSender.hpp"
#include "iomanager/Receiver.hpp"
#include "iomanager/Sender.hpp"
#include "utilities/WorkerThread.hpp"

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <string>
#include <thread>

#define private public
#define protected public

#include "CIBModule.hpp"

#undef protected
#undef private

#include "CIBModule.cpp"

namespace {

template<class Action>
std::string
run_with_mock_control_server(dunedaq::cibmodules::CIBModule& module,
                             const std::string& server_reply,
                             Action&& client_action)
{
  std::promise<unsigned short> port_promise;
  auto port_future = port_promise.get_future();

  std::promise<std::string> request_promise;
  auto request_future = request_promise.get_future();

  std::thread server_thread([reply = server_reply,
                             port_promise = std::move(port_promise),
                             request_promise = std::move(request_promise)]() mutable {
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::acceptor acceptor(io_service, { boost::asio::ip::tcp::v4(), 0 });
    acceptor.listen();
    port_promise.set_value(acceptor.local_endpoint().port());

    boost::asio::ip::tcp::socket socket(io_service);
    acceptor.accept(socket);

    std::array<char, 4096> buffer{};
    boost::system::error_code error;
    const auto received = socket.read_some(boost::asio::buffer(buffer), error);
    request_promise.set_value(std::string(buffer.data(), received));

    boost::asio::write(socket, boost::asio::buffer(reply), error);
    socket.close();
  });

  module.m_control_socket.connect(
    { boost::asio::ip::make_address("127.0.0.1"), port_future.get() });
  client_action();
  module.m_control_socket.close();

  server_thread.join();
  return request_future.get();
}

template<class ServerAction, class ClientAction>
void
run_with_connected_pair(ServerAction&& server_action, ClientAction&& client_action)
{
  std::promise<unsigned short> port_promise;
  auto port_future = port_promise.get_future();

  std::thread server_thread([&]() {
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::acceptor acceptor(io_service, { boost::asio::ip::tcp::v4(), 0 });
    acceptor.listen();
    port_promise.set_value(acceptor.local_endpoint().port());

    boost::asio::ip::tcp::socket socket(io_service);
    acceptor.accept(socket);
    server_action(socket);
  });

  boost::asio::io_service client_io_service;
  boost::asio::ip::tcp::socket client_socket(client_io_service);
  client_socket.connect(
    { boost::asio::ip::make_address("127.0.0.1"), port_future.get() });
  client_action(client_socket);

  server_thread.join();
}

}  // namespace ""

BOOST_AUTO_TEST_SUITE(CIBModule_test)

BOOST_AUTO_TEST_CASE(ConstructsWithNoErrorState)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");
  BOOST_REQUIRE(!module.error_state());
}

BOOST_AUTO_TEST_CASE(BufferCountHelpersBehaveAsExpected)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");

  BOOST_REQUIRE_EQUAL(module.read_average_buffer_counts(), 0.0);

  module.update_buffer_counts(2);
  module.update_buffer_counts(4);
  module.update_buffer_counts(6);

  BOOST_REQUIRE_EQUAL(module.read_average_buffer_counts(), 4.0);

  for (unsigned int index = 0; index < 1205; ++index) {
    module.update_buffer_counts(1);
  }

  BOOST_REQUIRE(module.m_buffer_counts.size() <= 1001);
}

BOOST_AUTO_TEST_CASE(CalibrationStreamFormattingAndRotationLogic)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");

  module.m_calibration_dir = "/tmp/cibmodule-tests";
  BOOST_REQUIRE(module.set_calibration_stream("run7"));
  BOOST_REQUIRE_EQUAL(module.m_calibration_dir.back(), '/');
  BOOST_REQUIRE_EQUAL(module.m_calibration_prefix, "run7_");

  module.m_calibration_stream_enable = true;
  module.m_calibration_file_interval = std::chrono::minutes(1);
  module.m_last_calibration_file_update = std::chrono::steady_clock::now();
  module.update_calibration_file();

  module.m_calibration_stream_enable = false;
  module.update_calibration_file();
}

BOOST_AUTO_TEST_CASE(CheckPortInUseFindsBusyAndFreePorts)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");

  boost::asio::io_service io_service;
  boost::asio::ip::tcp::acceptor acceptor(io_service);
  acceptor.open(boost::asio::ip::tcp::v4());
  acceptor.bind({ boost::asio::ip::tcp::v4(), 0 });
  const auto port = acceptor.local_endpoint().port();
  acceptor.listen();

  BOOST_REQUIRE(module.check_port_in_use(port));

  acceptor.close();
  BOOST_REQUIRE(!module.check_port_in_use(port));
}

BOOST_AUTO_TEST_CASE(SendMessageParsesFeedbackAndTracksCounters)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");

  const std::string reply =
    R"({"feedback":[{"type":"warning","message":"warn"},{"type":"info","message":"ok"},{"type":"misc","message":"blob"}]})";
  bool ret = false;

  const auto request = run_with_mock_control_server(module, reply, [&]() {
    ret = module.send_message("{\"command\":\"ping\"}");
  });

  BOOST_REQUIRE(ret);
  BOOST_REQUIRE(request.find("\"command\":\"ping\"") != std::string::npos);
  BOOST_REQUIRE_EQUAL(module.m_num_control_messages_sent.load(), 1);
  BOOST_REQUIRE_EQUAL(module.m_num_control_responses_received.load(), 3);
}

BOOST_AUTO_TEST_CASE(SendMessageReturnsFalseOnErrorFeedback)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");

  const std::string reply = R"({"feedback":[{"type":"ERROR","message":"failure"}]})";
  bool ret = true;

  run_with_mock_control_server(module, reply, [&]() {
    ret = module.send_message("{\"command\":\"stop\"}");
  });

  BOOST_REQUIRE(!ret);
  BOOST_REQUIRE_EQUAL(module.m_num_control_messages_sent.load(), 1);
  BOOST_REQUIRE_EQUAL(module.m_num_control_responses_received.load(), 1);
}

BOOST_AUTO_TEST_CASE(SendConfigWrapsConfigPayload)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");

  const std::string reply = R"({"feedback":[{"type":"info","message":"configured"}]})";
  const auto request = run_with_mock_control_server(module, reply, [&]() {
    module.send_config("{\"receiver\":\"host\",\"port\":1234}");
  });

  const auto sent_json = nlohmann::json::parse(request);
  BOOST_REQUIRE_EQUAL(sent_json["command"].get<std::string>(), "config");
  BOOST_REQUIRE_EQUAL(sent_json["config"]["receiver"].get<std::string>(), "host");
  BOOST_REQUIRE_EQUAL(sent_json["config"]["port"].get<int>(), 1234);
  BOOST_REQUIRE(module.m_is_configured.load());
}

BOOST_AUTO_TEST_CASE(ReadReturnsTrueWhenPayloadArrives)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");

  run_with_connected_pair(
    [](boost::asio::ip::tcp::socket& server_socket) {
      const std::uint32_t payload = 0x1234ABCD;
      boost::system::error_code error;
      boost::asio::write(server_socket, boost::asio::buffer(&payload, sizeof(payload)), error);
      server_socket.close();
    },
    [&](boost::asio::ip::tcp::socket& client_socket) {
      std::uint32_t payload = 0;
      BOOST_REQUIRE(module.read(client_socket, payload));
      BOOST_REQUIRE_EQUAL(payload, 0x1234ABCDU);
    });
}

BOOST_AUTO_TEST_CASE(ReadReturnsFalseOnEOFWhenStopping)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");
  module.m_stop_requested.store(true);

  run_with_connected_pair(
    [](boost::asio::ip::tcp::socket& server_socket) { server_socket.close(); },
    [&](boost::asio::ip::tcp::socket& client_socket) {
      std::uint32_t payload = 0;
      BOOST_REQUIRE(!module.read(client_socket, payload));
    });
}

BOOST_AUTO_TEST_CASE(ReadReturnsFalseOnSocketError)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::socket unconnected_socket(io_service);

  std::uint32_t payload = 0;
  BOOST_REQUIRE(!module.read(unconnected_socket, payload));
}

BOOST_AUTO_TEST_CASE(InitCalibrationFileDisablesStreamOnOpenFailure)
{
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");
  module.m_calibration_stream_enable = true;
  module.m_calibration_dir = "/definitely/not/a/real/path/";
  module.m_calibration_prefix = "run9_";

  module.init_calibration_file();

  BOOST_REQUIRE(!module.m_calibration_stream_enable);
}

BOOST_AUTO_TEST_CASE(InitAndRotateCalibrationFileOnWritablePath)
{
  namespace fs = std::filesystem;
  dunedaq::cibmodules::CIBModule module("cibmodule_unit_test");

  const fs::path temp_dir = fs::temp_directory_path() / "cibmodule_cov_tests";
  fs::create_directories(temp_dir);

  module.m_calibration_stream_enable = true;
  module.m_calibration_dir = temp_dir.string();
  module.m_calibration_prefix = "run10_";

  module.init_calibration_file();
  BOOST_REQUIRE(module.m_calibration_stream_enable);
  BOOST_REQUIRE(module.m_calibration_file.is_open());

  module.m_calibration_file_interval = std::chrono::minutes(0);
  module.update_calibration_file();
  BOOST_REQUIRE(module.m_calibration_file.is_open());

  module.m_calibration_file.close();
  fs::remove_all(temp_dir);
}

BOOST_AUTO_TEST_SUITE_END()
