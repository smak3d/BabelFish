#include "NetworkServer.h"

#include <utility>

namespace
{
    int toZmqTimeout(int milliseconds)
    {
        return milliseconds < 0 ? -1 : milliseconds;
    }

    std::string timeoutText(const char* operation, int milliseconds)
    {
        if (milliseconds < 0)
            return std::string(operation) + " timed out";

        return std::string(operation) + " timed out after "
             + std::to_string(milliseconds) + " ms";
    }
}

NetworkServer::NetworkServer() = default;

NetworkServer::~NetworkServer()
{
    close();
}

bool NetworkServer::listen(std::uint16_t port)
{
    return listen("tcp://*:" + std::to_string(port));
}

bool NetworkServer::listen(const std::string& endpoint)
{
    close(); // старый сокет (и старый bind) не должен пережить новый

    try
    {
        socket = std::make_unique<zmq::socket_t>(context, zmq::socket_type::rep);

        // LINGER=0 — как у клиента: закрытие после ошибки не должно ждать.
        socket->set(zmq::sockopt::linger, 0);
        socket->set(zmq::sockopt::rcvtimeo, toZmqTimeout(receiveTimeoutMs));
        socket->set(zmq::sockopt::sndtimeo, toZmqTimeout(sendTimeoutMs));

        socket->bind(endpoint);
    }
    catch (const zmq::error_t& exception)
    {
        return fail(NetworkError::SocketError, exception.what());
    }

    error = NetworkError::Ok;
    errorText.clear();
    return true;
}

void NetworkServer::close()
{
    socket.reset();
}

bool NetworkServer::isListening() const
{
    return socket != nullptr;
}

std::optional<zmq::message_t> NetworkServer::receive()
{
    if (!socket)
    {
        fail(NetworkError::NotConnected, "not listening: call listen() first");
        return std::nullopt;
    }

    zmq::message_t message;
    try
    {
        const auto received = socket->recv(message);
        if (!received)
        {
            fail(NetworkError::Timeout, timeoutText("receive", receiveTimeoutMs));
            return std::nullopt;
        }
    }
    catch (const zmq::error_t& exception)
    {
        fail(NetworkError::SocketError, exception.what());
        return std::nullopt;
    }

    error = NetworkError::Ok;
    errorText.clear();
    return message;
}

bool NetworkServer::send(zmq::message_t message)
{
    if (!socket)
        return fail(NetworkError::NotConnected, "not listening: call listen() first");

    try
    {
        const auto sent = socket->send(std::move(message), zmq::send_flags::none);
        if (!sent)
            return fail(NetworkError::Timeout, timeoutText("send", sendTimeoutMs));
    }
    catch (const zmq::error_t& exception)
    {
        // EFSM здесь означает «ответ без принятого запроса» — нарушение
        // очерёдности REP, чинится только пересозданием сокета.
        return fail(NetworkError::SocketError, exception.what());
    }

    error = NetworkError::Ok;
    errorText.clear();
    return true;
}

void NetworkServer::setReceiveTimeout(int milliseconds)
{
    receiveTimeoutMs = milliseconds;
    applyTimeouts();
}

void NetworkServer::setSendTimeout(int milliseconds)
{
    sendTimeoutMs = milliseconds;
    applyTimeouts();
}

int NetworkServer::receiveTimeout() const
{
    return receiveTimeoutMs;
}

int NetworkServer::sendTimeout() const
{
    return sendTimeoutMs;
}

NetworkError NetworkServer::lastError() const
{
    return error;
}

const std::string& NetworkServer::lastErrorText() const
{
    return errorText;
}

bool NetworkServer::fail(NetworkError failure, std::string text)
{
    error = failure;
    errorText = std::move(text);
    socket.reset();
    return false;
}

void NetworkServer::applyTimeouts()
{
    if (!socket)
        return; // без сокета значение переждёт до listen()

    try
    {
        socket->set(zmq::sockopt::rcvtimeo, toZmqTimeout(receiveTimeoutMs));
        socket->set(zmq::sockopt::sndtimeo, toZmqTimeout(sendTimeoutMs));
    }
    catch (const zmq::error_t& exception)
    {
        fail(NetworkError::SocketError, exception.what());
    }
}
