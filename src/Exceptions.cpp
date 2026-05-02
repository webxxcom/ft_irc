#include "Exceptions.hpp"

ServerException::ServerException(std::string const& msg) : std::runtime_error(msg) { }

ServerErrorException::ServerErrorException(std::string const& msg) : ServerException(msg) {}
