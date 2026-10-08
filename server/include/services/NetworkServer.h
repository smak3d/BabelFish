#pragma once

#include "NetworkError.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <zmq.hpp>

// Транспорт сервера: REP-сокет ZeroMQ поверх TCP.
//
// Контракт совпадает с клиентским (см. NetworkClient.h) и держать их вместе
// — не случайность: если концы по-разному трактуют ошибки, событие на
// клиенте и запись в логе сервера разойдутся в показаниях.
//
//   - listen() -> receive() -> send() -> receive() -> ... : REP обязан
//     ответить на принятый запрос, иначе zmq вернёт EFSM — это
//     NetworkError::SocketError;
//   - любая ошибка закрывает сокет, дальше только listen() заново. Это
//     штатное восстановление, а не костыль: TransportEcho проверяет, что
//     тот же порт слушается заново сразу после close().
//
// Приём по умолчанию бесконечный: клиент может долго записывать аудио, прежде
// чем отправит первый запрос, и разрывать из-за этого соединение нельзя.
class NetworkServer
{
    public:
        NetworkServer();
        ~NetworkServer();

        NetworkServer(const NetworkServer&) = delete;
        NetworkServer& operator=(const NetworkServer&) = delete;

        // Слушать все интерфейсы: tcp://*:<port>. Именно так сервер и
        // должен работать на второй машине.
        bool listen(std::uint16_t port);

        // Слушать конкретный адрес — tcp://127.0.0.1:<port> для тестов, чтобы
        // проверка не дёргала брандмауэр и не зависела от чужих портов.
        bool listen(const std::string& endpoint);

        // Закрыть сокет (и перестать слушать). lastError() не трогает.
        void close();

        bool isListening() const;

        // Принять один кадр. nullopt = смотреть lastError()/lastErrorText().
        std::optional<zmq::message_t> receive();

        // Ответить на принятый запрос. Содержимое переезжает в zmq.
        bool send(zmq::message_t message);

        // Таймауты в миллисекундах: больше нуля — ждать столько, меньше
        // нуля — бесконечно. На живом сокете применяются сразу.
        void setReceiveTimeout(int milliseconds);
        void setSendTimeout(int milliseconds);

        int receiveTimeout() const;
        int sendTimeout() const;

        NetworkError lastError() const;
        const std::string& lastErrorText() const;

    private:
        // Единственная точка отказа: записать причину и закрыть сокет.
        bool fail(NetworkError error, std::string text);
        void applyTimeouts();

        // Контекст объявлен раньше сокета — см. комментарий в NetworkClient.h.
        zmq::context_t context;
        std::unique_ptr<zmq::socket_t> socket;

        NetworkError error = NetworkError::Ok;
        std::string errorText;

        int receiveTimeoutMs = -1;      // ждать запроса бесконечно
        int sendTimeoutMs = 10'000;
};
