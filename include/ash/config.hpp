#ifndef ASH_CONFIG_H
#define ASH_CONFIG_H

#if defined(BOOST_CAMPBELL)
#include <boost/asio.hpp>
#else
#include <asio.hpp>
#endif

namespace ash
{

#if defined(BOOST_CAMPBELL)
namespace net = boost::asio;
#else
namespace net = asio;
#endif

}

#endif //ASH_CONFIG_H
