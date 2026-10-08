#include "NetworkClient.h"

#include <utility>

namespace
{
    // Таймаут в zmq — миллисекунды, где -1 значит «ждать бесконечно».
    int toZmqTimeout(int milliseconds)
    {
        return milliseconds < 0 ? -1 : milliseconds;
    }

    // При EAGAIN в тексте полезно видеть, сколько мы готовы были ждать:
    // «receive timed out» без числа на двухмашинаной отладке ничего не говорит.
    std::string timeoutText(const char* operation, int milliseconds)
    {
        if (milliseconds < 0)
            return std::string(operation) + " timed out";

        return std::string(operation) + " timed out after "
             + std::to_string(milliseconds) + " ms";
    }
}

NetworkClient::NetworkClient() = default;

NetworkClient::~NetworkClient()
{
    disconnect();
}

bool NetworkClient::connect(const std::string& host, std::uint16_t port)
{
    disconnect(); // старый сокет не должен пережить новое подключение

    try
    {
        socket = std::make_unique<zmq::socket_t>(context, zmq::socket_type::req);

        // LINGER=0: закрытие не ждёт недосланных кадров. Без этого обрыв на
        // недоступном сервере превращается в подвисание в disconnect() —
        // а закрываем мы сокет именно после ошибок.
        socket->set(zmq::sockopt::linger, 0);
        socket->set(zmq::sockopt::rcvtimeo, toZmqTimeout(receiveTimeoutMs));
        socket->set(zmq::sockopt::sndtimeo, toZmqTimeout(sendTimeoutMs));

        socket->connect("tcp://" + host + ":" + std::to_string(port));
    }
    catch (const zmq::error_t& exception)
    {
        return fail(NetworkError::SocketError, exception.what());
    }

    error = NetworkError::Ok;
    errorText.clear();
    return true;
}

void NetworkClient::disconnect()
{
    socket.reset();
}

bool NetworkClient::isConnected() const
{
    return socket != nullptr;
}

bool NetworkClient::send(zmq::message_t message)
{
    if (!socket)
        return fail(NetworkError::NotConnected, "not connected: call connect() first");

    try
    {
        const auto sent = socket->send(std::move(message), zmq::send_flags::none);
        if (!sent)
            return fail(NetworkError::Timeout, timeoutText("send", sendTimeoutMs));
    }
    catch (const zmq::error_t& exception)
    {
        // EFSM — «второй запрос до ответа на первый» — сюда же: продолжать
        // в таком состоянии нельзя, восстановление только через connect().
        return fail(NetworkError::SocketError, exception.what());
    }

    error = NetworkError::Ok;
    errorText.clear();
    return true;
}

std::optional<zmq::message_t> NetworkClient::receive()
{
    if (!socket)
    {
        fail(NetworkError::NotConnected, "not connected: call connect() first");
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

void NetworkClient::setReceiveTimeout(int milliseconds)
{
    receiveTimeoutMs = milliseconds;
    applyTimeouts();
}

void NetworkClient::setSendTimeout(int milliseconds)
{
    sendTimeoutMs = milliseconds;
    applyTimeouts();
}

int NetworkClient::receiveTimeout() const
{
    return receiveTimeoutMs;
}

int NetworkClient::sendTimeout() const
{
    return sendTimeoutMs;
}

NetworkError NetworkClient::lastError() const
{
    return error;
}

const std::string& NetworkClient::lastErrorText() const
{
    return errorText;
}

bool NetworkClient::fail(NetworkError failure, std::string text)
{
    error = failure;
    errorText = std::move(text);

    // Правило одно на все ошибки: сокет после отказа не переиспользуем.
    // Подробности — в заголовке.
    socket.reset();

    return false;
}

void NetworkClient::applyTimeouts()
{
    if (!socket)
        return; // без сокета значение просто переждёт до connect()

    try
    {
        socket->set(zmq::sockopt::rcvtimeo, toZmqTimeout(receiveTimeoutMs));
        socket->set(zmq::sockopt::sndtimeo, toZmqTimeout(sendTimeoutMs));
    }
    catch (const zmq::error_t& exception)
    {
        // Отказ в setsockopt означает, что соединением что-то не так —
        // по общему правилу закрываем его.
        fail(NetworkError::SocketError, exception.what());
    }
}
