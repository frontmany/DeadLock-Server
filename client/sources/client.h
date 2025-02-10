#pragma once

#include <asio.hpp>
#include <thread>
#include <mutex>
#include <string>
#include <unordered_map>



#include "sender.h"
#include "workerUI.h"
#include "chat.h"



enum class OperationResult { NOT_STATED, FAIL, SUCCESS, REQUEST_TIMEOUT };
class Photo;

class Client {
public:
    
    Client();
    void init();
    void connectTo(const std::string& ipAddress, int port);
    void setWorkerUI(WorkerUI* workerImpl);
    void close();

    OperationResult authorizeClient(const std::string& login, const std::string& password);
    OperationResult registerClient(const std::string& login, const std::string& password, const std::string& name);
    OperationResult updateMyInfo(const std::string& login, const std::string& name, const std::string& password, bool isHasPhoto, Photo photo = Photo());
    OperationResult createChatWith(const std::string& friendLogin);
    OperationResult sendMessage(const std::string& friendLogin, const std::string& message, const std::string timestamp);
    OperationResult sendMyStatus(const std::string& status);
    OperationResult sendMessageReadConfirmation(const std::string& friendLogin, const std::vector<int>& messagesReadIdsVec);


    void setMyLogin(const std::string& login) { m_my_login = login; }
    const std::string& getMyLogin() const { return m_my_login; }

    void setMyName(const std::string& name) { m_my_name = name; }
    const std::string& getMyName() const { return m_my_name; }

    void setMyPassword(const std::string& password) { m_my_password = password; }
    const std::string& getMyPassword() const { return m_my_password; }

private:
    void receive();
    void sendPacket(const std::string& packet);
    void handleResponse(const std::string& packet);
    OperationResult waitForResponse(int seconds, std::function<OperationResult()> checkFunction);

private:
    const std::string       c_endPacket = "_+14?bb5HmR;%@`7[S^?!#sL8";
    asio::io_context        m_io_context;
    asio::ip::tcp::socket   m_socket;
    std::thread             m_receiverThread;
    std::mutex              m_mtx;
    bool                    m_isReceiving;
    SendStringsGenerator    m_sender;
    WorkerUI*               m_worker;

    std::string m_my_login = "";
    std::string m_my_name = "";
    std::string m_my_password = "";
    bool m_is_has_photo;
    Photo m_my_photo;
    std::unordered_map<std::string, Chat*> m_map_friend_login_to_chat;

    //shared variables (main thread await until state changed on FAIL, SUCCESS or REQUEST_TIMEOUT)
    OperationResult     sh_is_authorized;
    OperationResult     sh_is_info_updated;
    OperationResult     sh_chat_create;
    OperationResult     sh_is_status_send;
    OperationResult     sh_is_message_send;
    OperationResult     sh_is_message_read_confirmation_send;
};