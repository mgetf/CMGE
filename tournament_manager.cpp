#include "tournament_manager.hpp"
#include <curl/curl.h>
#include <libwebsockets.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace mge
{

  static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
  {
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
  }

  ChallongeAPI::ChallongeAPI(const std::string &user, const std::string &key,
                             const std::string &subdom, const std::string &tournamentUrl)
      : username(user), apiKey(key), subdomain(subdom), tournamentUrl(tournamentUrl)
  {
    loadTournament();
  }

  std::string ChallongeAPI::makeRequest(const std::string &method,
                                        const std::string &endpoint,
                                        const json &data)
  {
    CURL *curl = curl_easy_init();
    std::string response;

    if (!curl)
    {
      std::cerr << "Failed to initialize CURL" << std::endl;
      return "";
    }

    std::string url = "https://api.challonge.com/v1" + endpoint;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    json requestData = data;
    requestData["api_key"] = apiKey;

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (method == "POST")
    {
      std::string jsonStr = requestData.dump();
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    }
    else if (method == "PUT")
    {
      std::string jsonStr = requestData.dump();
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    }
    else if (method == "GET")
    {
      std::string params = "?api_key=" + apiKey;
      for (auto &[key, value] : requestData.items())
      {
        if (key != "api_key")
        {
          params += "&" + key + "=" + value.get<std::string>();
        }
      }
      url += params;
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
      std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
  }

  void ChallongeAPI::loadTournament()
  {
    std::string endpoint = "/tournaments/" + subdomain + "-" + tournamentUrl + ".json";
    json params = {{"include_participants", "1"}, {"include_matches", "1"}};

    std::string response = makeRequest("GET", endpoint, params);

    try
    {
      json j = json::parse(response);
      if (j.contains("tournament") && j["tournament"].contains("id"))
      {
        tournamentId = std::to_string(j["tournament"]["id"].get<int>());
        std::cout << "Loaded tournament: " << tournamentId << std::endl;
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error parsing tournament: " << e.what() << std::endl;
    }
  }

  void ChallongeAPI::addParticipant(const std::string &name,
                                    const std::string &steamId, int seed)
  {
    std::string endpoint = "/tournaments/" + tournamentId + "/participants.json";
    json data = {
        {"participant", {{"name", name}, {"seed", seed}, {"misc", steamId}}}};

    std::string response = makeRequest("POST", endpoint, data);
    std::cout << "Added participant: " << name << std::endl;
  }

  void ChallongeAPI::startTournament()
  {
    std::string endpoint = "/tournaments/" + tournamentId + "/start.json";
    std::string response = makeRequest("POST", endpoint);
    std::cout << "Started tournament" << std::endl;
  }

  std::vector<PendingMatch> ChallongeAPI::getPendingMatches()
  {
    std::string endpoint = "/tournaments/" + tournamentId + "/matches.json";
    json params = {{"state", "open"}};

    std::string response = makeRequest("GET", endpoint, params);
    std::vector<PendingMatch> matches;

    try
    {
      json matchesJson = json::parse(response);

      std::string participantsEndpoint = "/tournaments/" + tournamentId + "/participants.json";
      std::string participantsResponse = makeRequest("GET", participantsEndpoint);
      json participantsJson = json::parse(participantsResponse);

      std::map<int, std::pair<std::string, std::string>> idToPlayer;

      for (auto &p : participantsJson)
      {
        if (p.contains("participant"))
        {
          auto &participant = p["participant"];
          int id = participant["id"].get<int>();
          std::string name = participant["name"].get<std::string>();
          std::string steamId = participant.contains("misc") ? participant["misc"].get<std::string>() : "";
          idToPlayer[id] = {name, steamId};
        }
      }

      for (auto &m : matchesJson)
      {
        if (m.contains("match"))
        {
          auto &match = m["match"];

          if (match.contains("player1_id") && match.contains("player2_id") &&
              !match["player1_id"].is_null() && !match["player2_id"].is_null())
          {

            if (!match["winner_id"].is_null())
              continue;

            int p1Id = match["player1_id"].get<int>();
            int p2Id = match["player2_id"].get<int>();

            if (idToPlayer.count(p1Id) && idToPlayer.count(p2Id))
            {
              PendingMatch pm;
              pm.player1Name = idToPlayer[p1Id].first;
              pm.player1Id = idToPlayer[p1Id].second;
              pm.player2Name = idToPlayer[p2Id].first;
              pm.player2Id = idToPlayer[p2Id].second;
              matches.push_back(pm);
            }
          }
        }
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error parsing pending matches: " << e.what() << std::endl;
    }

    return matches;
  }

  void ChallongeAPI::reportMatch(const std::string &winnerId, const std::string &loserId)
  {
    std::string matchesEndpoint = "/tournaments/" + tournamentId + "/matches.json";
    json params = {{"state", "open"}};
    std::string response = makeRequest("GET", matchesEndpoint, params);

    try
    {
      json matchesJson = json::parse(response);

      std::string participantsEndpoint = "/tournaments/" + tournamentId + "/participants.json";
      std::string participantsResponse = makeRequest("GET", participantsEndpoint);
      json participantsJson = json::parse(participantsResponse);

      std::map<std::string, int> steamIdToParticipantId;
      for (auto &p : participantsJson)
      {
        if (p.contains("participant"))
        {
          auto &participant = p["participant"];
          if (participant.contains("misc"))
          {
            std::string steamId = participant["misc"].get<std::string>();
            int id = participant["id"].get<int>();
            steamIdToParticipantId[steamId] = id;
          }
        }
      }

      if (!steamIdToParticipantId.count(winnerId) || !steamIdToParticipantId.count(loserId))
      {
        std::cerr << "Could not find participant IDs for match" << std::endl;
        return;
      }

      int winnerParticipantId = steamIdToParticipantId[winnerId];
      int loserParticipantId = steamIdToParticipantId[loserId];

      for (auto &m : matchesJson)
      {
        if (m.contains("match"))
        {
          auto &match = m["match"];

          if (!match["player1_id"].is_null() && !match["player2_id"].is_null())
          {
            int p1Id = match["player1_id"].get<int>();
            int p2Id = match["player2_id"].get<int>();

            if ((p1Id == winnerParticipantId && p2Id == loserParticipantId) ||
                (p2Id == winnerParticipantId && p1Id == loserParticipantId))
            {

              int matchId = match["id"].get<int>();
              std::string updateEndpoint = "/tournaments/" + tournamentId +
                                           "/matches/" + std::to_string(matchId) + ".json";

              std::string scoreCsv = (p1Id == winnerParticipantId) ? "1-0" : "0-1";
              json updateData = {
                  {"match", {{"scores_csv", scoreCsv}, {"winner_id", winnerParticipantId}}}};

              makeRequest("PUT", updateEndpoint, updateData);
              std::cout << "Reported match result" << std::endl;
              return;
            }
          }
        }
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error reporting match: " << e.what() << std::endl;
    }
  }

  TournamentManager::TournamentManager(lws_context *ctx, const std::string &challongeUser,
                                       const std::string &challongeKey, const std::string &tournamentUrl)
      : context(ctx), arenas(NUM_ARENAS), mgeClientWsi(nullptr), mgeConnected(false), tournamentActive(false)
  {

    arenaPriority = {5, 6, 7, 1, 2, 3, 4, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    std::string subdomain = "89c2a59aadab1761b8e29117";
    challonge = std::make_unique<ChallongeAPI>(challongeUser, challongeKey, subdomain, tournamentUrl);
  }

  void TournamentManager::addConnection(lws *wsi)
  {
    connections[wsi] = std::make_unique<WebSocketConnection>();
    connections[wsi]->wsi = wsi;
  }

  void TournamentManager::removeConnection(lws *wsi)
  {
    if (connections.count(wsi))
    {
      if (connections[wsi].get() == admin)
      {
        admin = nullptr;
      }
      connections.erase(wsi);
    }
  }

  void TournamentManager::queueMessage(lws *wsi, const std::string &message)
  {
    if (connections.count(wsi))
    {
      connections[wsi]->messageQueue.push_back(message);
      lws_callback_on_writable(wsi);
    }
  }

  bool TournamentManager::hasQueuedMessages(lws *wsi) const
  {
    if (connections.count(wsi))
    {
      return !connections.at(wsi)->messageQueue.empty();
    }
    return false;
  }

  std::string TournamentManager::popMessage(lws *wsi)
  {
    if (connections.count(wsi) && !connections[wsi]->messageQueue.empty())
    {
      std::string msg = connections[wsi]->messageQueue.front();
      connections[wsi]->messageQueue.erase(connections[wsi]->messageQueue.begin());
      return msg;
    }
    return "";
  }

  std::optional<int> TournamentManager::getOpenArena()
  {
    for (int arenaId : arenaPriority)
    {
      if (arenas[arenaId - 1].isEmpty())
      {
        return arenaId - 1;
      }
    }
    return std::nullopt;
  }

  bool TournamentManager::isPlayerInMatch(const std::string &steamId) const
  {
    for (const auto &arena : arenas)
    {
      if (arena.hasPlayer(steamId))
      {
        return true;
      }
    }
    return false;
  }

  void TournamentManager::broadcastToServers(const json &message)
  {
    std::string msgStr = message.dump();

    for (auto &[wsi, conn] : connections)
    {
      if (conn->type == "server")
      {
        queueMessage(wsi, msgStr);
      }
    }
  }

  void TournamentManager::sendToConnection(WebSocketConnection *conn, const json &message)
  {
    if (conn)
    {
      queueMessage(conn->wsi, message.dump());
    }
  }

  void TournamentManager::sendToMGEPlugin(const json &message)
  {
    if (!mgeConnected || !mgeClientWsi)
    {
      std::cerr << "Cannot send to MGE plugin: not connected" << std::endl;
      return;
    }

    std::string msgStr = message.dump();
    mgeOutgoingMessages.push(msgStr);
    lws_callback_on_writable(mgeClientWsi);
  }

  void TournamentManager::requestPlayersFromMGE()
  {
    json request = {{"command", "get_players"}};
    sendToMGEPlugin(request);
  }

  void TournamentManager::requestArenasFromMGE()
  {
    json request = {{"command", "get_arenas"}};
    sendToMGEPlugin(request);
  }

  void TournamentManager::addPlayerToMGEArena(int clientId, int arenaId)
  {
    json request = {
        {"command", "add_player_to_arena"},
        {"player_id", clientId},
        {"arena_id", arenaId}};
    sendToMGEPlugin(request);
  }

  void TournamentManager::handleMGEPluginMessage(const std::string &message)
  {
    try
    {
      json j = json::parse(message);

      if (!j.contains("type"))
      {
        return;
      }

      std::string type = j["type"];

      if (type == "welcome")
      {
        std::cout << "Connected to MGE plugin: " << j.value("message", "") << std::endl;
        requestArenasFromMGE();
        requestPlayersFromMGE();
      }
      else if (type == "response")
      {
        std::string command = j.value("command", "");

        if (command == "get_players")
        {
          players.clear();
          steamIdToClientId.clear();
          clientIdToSteamId.clear();

          if (j.contains("players"))
          {
            for (const auto &p : j["players"])
            {
              Player player;
              player.clientId = p.value("id", 0);
              player.name = p.value("name", "");
              player.arena = p.value("arena", 0);
              player.inArena = p.value("inArena", false);
              player.elo = p.value("elo", 1000);

              char steamIdBuf[64];
              snprintf(steamIdBuf, sizeof(steamIdBuf), "STEAM_ID_%d", player.clientId);
              player.steamId = steamIdBuf;

              steamIdToClientId[player.steamId] = player.clientId;
              clientIdToSteamId[player.clientId] = player.steamId;

              players.push_back(player);
            }
            std::cout << "Received " << players.size() << " players from MGE plugin" << std::endl;
          }
        }
        else if (command == "get_arenas")
        {
          std::cout << "Received arena info from MGE plugin" << std::endl;
        }
      }
      else if (type == "event")
      {
        handleMGEEvent(j);
      }
      else if (type == "success")
      {
        std::cout << "MGE Plugin Success: " << j.value("message", "") << std::endl;
      }
      else if (type == "error")
      {
        std::cerr << "MGE Plugin Error: " << j.value("message", "") << std::endl;
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error handling MGE plugin message: " << e.what() << std::endl;
    }
  }

  void TournamentManager::handleMGEEvent(const json &event)
  {
    std::string eventType = event.value("event", "");

    if (eventType == "match_end_1v1")
    {
      int winnerId = event.value("winner_id", 0);
      int loserId = event.value("loser_id", 0);
      int arenaId = event.value("arena_id", 0);

      if (clientIdToSteamId.count(winnerId) && clientIdToSteamId.count(loserId))
      {
        std::string winnerSteamId = clientIdToSteamId[winnerId];
        std::string loserSteamId = clientIdToSteamId[loserId];

        std::cout << "Match ended: " << event.value("winner_name", "")
                  << " beat " << event.value("loser_name", "") << std::endl;

        if (tournamentActive)
        {
          challonge->reportMatch(winnerSteamId, loserSteamId);

          if (arenaId > 0 && arenaId <= NUM_ARENAS)
          {
            arenas[arenaId - 1].clear();
          }

          assignPendingMatches();
        }
      }
    }
    else if (eventType == "player_arena_removed")
    {
      int arenaId = event.value("arena_id", 0);
      if (arenaId > 0 && arenaId <= NUM_ARENAS)
      {
        int playerId = event.value("player_id", 0);
        if (clientIdToSteamId.count(playerId))
        {
          std::string steamId = clientIdToSteamId[playerId];
          arenas[arenaId - 1].currentMatch.reset();
        }
      }
    }
  }

  void TournamentManager::assignPendingMatches()
  {
    if (!mgeConnected)
    {
      std::cout << "Cannot assign matches: not connected to MGE plugin" << std::endl;
      return;
    }

    auto pendingMatches = challonge->getPendingMatches();

    for (const auto &match : pendingMatches)
    {
      if (isPlayerInMatch(match.player1Id) || isPlayerInMatch(match.player2Id))
      {
        continue;
      }

      auto arenaOpt = getOpenArena();
      if (!arenaOpt)
      {
        std::cout << "No open arenas available" << std::endl;
        break;
      }

      int arenaId = arenaOpt.value();
      std::set<std::string> matchPlayers = {match.player1Id, match.player2Id};
      arenas[arenaId].currentMatch = matchPlayers;

      if (steamIdToClientId.count(match.player1Id) && steamIdToClientId.count(match.player2Id))
      {
        int client1 = steamIdToClientId[match.player1Id];
        int client2 = steamIdToClientId[match.player2Id];

        addPlayerToMGEArena(client1, arenaId + 1);
        addPlayerToMGEArena(client2, arenaId + 1);

        std::cout << "Assigned match: " << match.player1Name << " vs "
                  << match.player2Name << " to arena " << (arenaId + 1) << std::endl;
      }
    }
  }

  void TournamentManager::handleMessage(lws *wsi, const std::string &message)
  {
    try
    {
      json j = json::parse(message);

      if (!j.contains("type"))
      {
        std::cerr << "Message missing 'type' field" << std::endl;
        return;
      }

      std::string type = j["type"];
      json payload = j.contains("payload") ? j["payload"] : json::object();

      std::cout << "Received: " << type << std::endl;

      if (type == "ServerHello")
      {
        handleServerHello(wsi, payload);
      }
      else if (type == "TournamentStart")
      {
        handleTournamentStart(payload);
      }
      else if (type == "TournamentStop")
      {
        handleTournamentStop(payload);
      }
      else if (type == "UsersInServer")
      {
        handleUsersInServer(payload);
      }
      else if (type == "MatchResults")
      {
        handleMatchResults(payload);
      }
      else if (type == "MatchBegan")
      {
        handleMatchBegan(payload);
      }
      else if (type == "MatchDetails")
      {
        handleMatchDetails(payload);
      }
      else if (type == "SetMatchScore")
      {
        handleSetMatchScore(wsi, payload);
      }
      else if (type == "MatchCancel")
      {
        handleMatchCancel(payload);
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error handling message: " << e.what() << std::endl;

      json errorMsg = {
          {"type", "Error"},
          {"payload", {{"message", e.what()}}}};
      queueMessage(wsi, errorMsg.dump());
    }
  }

  void TournamentManager::handleServerHello(lws *wsi, const json &payload)
  {
    if (!connections.count(wsi))
      return;

    std::string apiKey = payload.value("apiKey", "");

    if (apiKey == "admin")
    {
      connections[wsi]->type = "admin";
      admin = connections[wsi].get();
      std::cout << "Admin connected" << std::endl;
    }
    else
    {
      connections[wsi]->type = "server";
      std::cout << "Server connected" << std::endl;
    }
  }

  void TournamentManager::handleTournamentStart(const json &payload)
  {
    std::cout << "Tournament starting" << std::endl;
    tournamentActive = true;

    requestPlayersFromMGE();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    int seed = 1;
    for (const auto &player : players)
    {
      std::cout << "Adding player to Challonge: " << player.name << std::endl;
      challonge->addParticipant(player.name, player.steamId, seed++);
    }

    challonge->startTournament();

    assignPendingMatches();

    json msg = {
        {"type", "TournamentStart"},
        {"payload", json::object()}};
    broadcastToServers(msg);
  }

  void TournamentManager::handleTournamentStop(const json &payload)
  {
    std::cout << "Tournament stopping" << std::endl;
    tournamentActive = false;

    for (auto &arena : arenas)
    {
      arena.clear();
    }

    json msg = {
        {"type", "TournamentStop"},
        {"payload", json::object()}};
    broadcastToServers(msg);
  }

  void TournamentManager::handleUsersInServer(const json &payload)
  {
    if (!payload.contains("players"))
      return;

    players.clear();

    for (const auto &p : payload["players"])
    {
      Player player;
      player.steamId = p.value("steamId", "");
      player.name = p.value("name", "");
      player.elo = p.value("elo", 1000);
      players.push_back(player);
    }

    std::sort(players.begin(), players.end(),
              [](const Player &a, const Player &b)
              { return a.elo > b.elo; });

    std::cout << "Received " << players.size() << " players" << std::endl;

    int seed = 1;
    for (const auto &player : players)
    {
      std::cout << "Adding player: " << player.name << std::endl;
      challonge->addParticipant(player.name, player.steamId, seed++);
    }

    challonge->startTournament();

    assignPendingMatches();
  }

  void TournamentManager::handleMatchResults(const json &payload)
  {
    std::string winner = payload.value("winner", "");
    std::string loser = payload.value("loser", "");
    int arena = payload.value("arena", 0) - 1;

    std::cout << "Match result: " << winner << " beat " << loser
              << " in arena " << (arena + 1) << std::endl;

    challonge->reportMatch(winner, loser);

    if (arena >= 0 && arena < NUM_ARENAS)
    {
      arenas[arena].clear();
    }

    assignPendingMatches();
  }

  void TournamentManager::handleMatchBegan(const json &payload)
  {
    std::string p1 = payload.contains("p1Id") ? payload["p1Id"].get<std::string>()
                                              : payload.value("p1", "");
    std::string p2 = payload.contains("p2Id") ? payload["p2Id"].get<std::string>()
                                              : payload.value("p2", "");

    std::cout << "Match began: " << p1 << " vs " << p2 << std::endl;
  }

  void TournamentManager::handleMatchDetails(const json &payload)
  {
    int arenaId = payload.value("arenaId", 0) - 1;
    std::string p1Id = payload.value("p1Id", "");
    std::string p2Id = payload.value("p2Id", "");

    if (arenaId >= 0 && arenaId < NUM_ARENAS)
    {
      std::set<std::string> matchPlayers = {p1Id, p2Id};
      arenas[arenaId].currentMatch = matchPlayers;

      json msg = {
          {"type", "MatchDetails"},
          {"payload", payload}};
      broadcastToServers(msg);
    }
  }

  void TournamentManager::handleSetMatchScore(lws *wsi, const json &payload)
  {
    json msg = {
        {"type", "SetMatchScore"},
        {"payload", payload}};
    broadcastToServers(msg);
  }

  void TournamentManager::handleMatchCancel(const json &payload)
  {
    int arena = payload.value("arena", 0) - 1;

    if (arena >= 0 && arena < NUM_ARENAS)
    {
      arenas[arena].clear();
      std::cout << "Match cancelled in arena " << (arena + 1) << std::endl;
    }
  }

  void TournamentManager::setMGEClientWsi(lws *wsi)
  {
    mgeClientWsi = wsi;
  }

  void TournamentManager::onMGEConnected()
  {
    mgeConnected = true;
    std::cout << "Connected to MGE plugin WebSocket server" << std::endl;
  }

  void TournamentManager::onMGEDisconnected()
  {
    mgeConnected = false;
    mgeClientWsi = nullptr;
    std::cout << "Disconnected from MGE plugin WebSocket server" << std::endl;
  }

  void TournamentManager::queueMGEMessage(const std::string &message)
  {
    mgeOutgoingMessages.push(message);
    if (mgeClientWsi)
    {
      lws_callback_on_writable(mgeClientWsi);
    }
  }

  bool TournamentManager::hasMGEQueuedMessages() const
  {
    return !mgeOutgoingMessages.empty();
  }

  std::string TournamentManager::popMGEMessage()
  {
    if (mgeOutgoingMessages.empty())
    {
      return "";
    }
    std::string msg = mgeOutgoingMessages.front();
    mgeOutgoingMessages.pop();
    return msg;
  }

}