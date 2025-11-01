#include "tournament_manager.hpp"
#include <libwebsockets.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>

static mge::TournamentManager* g_tournament = nullptr;

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason,
                        void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_HTTP: {
            char *requested_uri = (char *)in;
            
            std::string uri(requested_uri);
            std::string filepath;
            
            if (uri == "/" || uri == "/admin") {
                filepath = "static/admin.html";
            } else if (uri == "/index") {
                filepath = "static/index.html";
            } else {
                filepath = "static" + uri;
            }
            
            if (lws_serve_http_file(wsi, filepath.c_str(), 
                                   "text/html", NULL, 0) < 0) {
                return -1;
            }
            break;
        }
        
        case LWS_CALLBACK_HTTP_FILE_COMPLETION:
            return -1;
            
        default:
            break;
    }
    
    return 0;
}

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            std::cout << "WebSocket connection established" << std::endl;
            if (g_tournament) {
                g_tournament->addConnection(wsi);
            }
            break;
            
        case LWS_CALLBACK_CLOSED:
            std::cout << "WebSocket connection closed" << std::endl;
            if (g_tournament) {
                g_tournament->removeConnection(wsi);
            }
            break;
            
        case LWS_CALLBACK_RECEIVE:
            if (g_tournament && in && len > 0) {
                std::string message((char*)in, len);
                g_tournament->handleMessage(wsi, message);
            }
            break;
            
        case LWS_CALLBACK_SERVER_WRITEABLE:
            if (g_tournament && g_tournament->hasQueuedMessages(wsi)) {
                std::string msg = g_tournament->popMessage(wsi);
                
                if (!msg.empty()) {
                    unsigned char buf[LWS_PRE + 4096];
                    size_t msgLen = msg.size();
                    
                    if (msgLen > 4096) {
                        std::cerr << "Message too large: " << msgLen << std::endl;
                        break;
                    }
                    
                    memcpy(&buf[LWS_PRE], msg.c_str(), msgLen);
                    
                    int written = lws_write(wsi, &buf[LWS_PRE], msgLen, LWS_WRITE_TEXT);
                    
                    if (written < 0) {
                        std::cerr << "Error writing to websocket" << std::endl;
                        return -1;
                    }
                    
                    if (g_tournament->hasQueuedMessages(wsi)) {
                        lws_callback_on_writable(wsi);
                    }
                }
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

static int callback_mge_client(struct lws *wsi, enum lws_callback_reasons reason,
                               void *user, void *in, size_t len) {
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            std::cout << "MGE Plugin client connection established" << std::endl;
            if (g_tournament) {
                g_tournament->setMGEClientWsi(wsi);
                g_tournament->onMGEConnected();
            }
            break;
            
        case LWS_CALLBACK_CLIENT_CLOSED:
            std::cout << "MGE Plugin client connection closed" << std::endl;
            if (g_tournament) {
                g_tournament->onMGEDisconnected();
            }
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (g_tournament && in && len > 0) {
                std::string message((char*)in, len);
                g_tournament->handleMGEPluginMessage(message);
            }
            break;
            
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (g_tournament && g_tournament->hasMGEQueuedMessages()) {
                std::string msg = g_tournament->popMGEMessage();
                
                if (!msg.empty()) {
                    unsigned char buf[LWS_PRE + 4096];
                    size_t msgLen = msg.size();
                    
                    if (msgLen > 4096) {
                        std::cerr << "MGE message too large: " << msgLen << std::endl;
                        break;
                    }
                    
                    memcpy(&buf[LWS_PRE], msg.c_str(), msgLen);
                    
                    int written = lws_write(wsi, &buf[LWS_PRE], msgLen, LWS_WRITE_TEXT);
                    
                    if (written < 0) {
                        std::cerr << "Error writing to MGE plugin websocket" << std::endl;
                        return -1;
                    }
                    
                    if (g_tournament->hasMGEQueuedMessages()) {
                        lws_callback_on_writable(wsi);
                    }
                }
            }
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            std::cerr << "MGE Plugin connection error" << std::endl;
            if (g_tournament) {
                g_tournament->onMGEDisconnected();
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "http",
        callback_http,
        0,
        0,
    },
    {
        "tf2serverep",
        callback_websocket,
        0,
        4096,
    },
    {
        "mge-client",
        callback_mge_client,
        0,
        4096,
    },
    { NULL, NULL, 0, 0 }
};

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    content.erase(0, content.find_first_not_of(" \n\r\t"));
    content.erase(content.find_last_not_of(" \n\r\t") + 1);
    
    return content;
}

void connectToMGEPlugin(lws_context *context) {
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    
    ccinfo.context = context;
    ccinfo.address = "localhost";
    ccinfo.port = 9001;
    ccinfo.path = "/";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = "mge-client";
    ccinfo.ietf_version_or_minus_one = -1;
    
    struct lws *wsi = lws_client_connect_via_info(&ccinfo);
    if (!wsi) {
        std::cerr << "Failed to connect to MGE plugin" << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tournament_url>" << std::endl;
        return 1;
    }
    
    std::string tournamentUrl = argv[1];
    std::cout << "Tournament URL: " << tournamentUrl << std::endl;
    
    std::string apiKey = readFile("api_key.txt");
    if (apiKey.empty()) {
        std::cerr << "Error: Could not read api_key.txt" << std::endl;
        return 1;
    }
    
    std::string challongeUser = "ZeroSTF";
    
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = 8080;
    info.protocols = protocols;
    //info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
    
    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        std::cerr << "Failed to create libwebsockets context" << std::endl;
        return 1;
    }
    
    g_tournament = new mge::TournamentManager(context, challongeUser, apiKey, tournamentUrl);
    
    std::cout << "Server started on port 8080" << std::endl;
    std::cout << "WebSocket endpoint: ws://localhost:8080" << std::endl;
    
    std::cout << "Attempting to connect to MGE plugin on localhost:9001..." << std::endl;
    connectToMGEPlugin(context);
    
    int n = 0;
    while (n >= 0) {
        n = lws_service(context, 50);
    }
    
    delete g_tournament;
    lws_context_destroy(context);
    
    return 0;
}