#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <iostream>
#include <deque>
#include <stack>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <ikcp.h>


#include "log.h"



namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace asio = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using udp = boost::asio::ip::udp;
namespace ssl = boost::asio::ssl;
using sslStream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;