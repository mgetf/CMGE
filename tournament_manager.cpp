#include "tournament_manager.hpp"
#include <curl/curl.h>
#include <libwebsockets.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <sstream>

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

  std::string ChallongeAPI::flattenToForm(const json &j, CURL *curl, const std::string &prefix) const
  {
    std::vector<std::string> parts;

    for (auto it = j.begin(); it != j.end(); ++it)
    {
      std::string key = prefix.empty() ? it.key() : prefix + "[" + it.key() + "]";

      if (it.value().is_object())
      {
        std::string sub = flattenToForm(it.value(), curl, key);
        if (!sub.empty())
        {
          std::stringstream ss(sub);
          std::string item;
          while (std::getline(ss, item, '&'))
          {
            if (!item.empty())
            {
              parts.push_back(item);
            }
          }
        }
      }
      else if (it.value().is_null())
      {
        continue;
      }
      else
      {
        std::string value;
        if (it.value().is_string())
        {
          std::string raw = it.value().get<std::string>();
          char *escaped = curl_easy_escape(curl, raw.c_str(), raw.length());
          value = escaped;
          curl_free(escaped);
        }
        else if (it.value().is_number_integer())
        {
          value = std::to_string(it.value().get<int>());
        }
        else if (it.value().is_number_float())
        {
          value = std::to_string(it.value().get<double>());
        }
        else if (it.value().is_boolean())
        {
          value = it.value().get<bool>() ? "true" : "false";
        }
        else
        {
          continue;
        }

        parts.push_back(key + "=" + value);
      }
    }

    std::string res;
    for (size_t i = 0; i < parts.size(); ++i)
    {
      if (i > 0)
        res += "&";
      res += parts[i];
    }
    return res;
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

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    std::string auth = username + ":" + apiKey;
    curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());

    struct curl_slist *headers = nullptr;

    if (method == "POST" || method == "PUT")
    {
      std::string formData = flattenToForm(data, curl, "");

      headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, formData.c_str());

      if (method == "POST")
      {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
      }
      else
      {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
      }

      std::cout << "[DEBUG] " << method << " to " << url << std::endl;
      std::cout << "[DEBUG] Form data: " << formData << std::endl;
    }
    else if (method == "GET" || method == "DELETE")
    {
      std::string params = flattenToForm(data, curl, "");
      if (!params.empty())
      {
        url += "?" + params;
      }
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      if (method == "DELETE")
      {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
      }

      std::cout << "[DEBUG] " << method << " to " << url << std::endl;
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
      std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
    }
    else
    {
      long response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      std::cout << "[DEBUG] HTTP response code: " << response_code << std::endl;

      if (response_code >= 400)
      {
        std::cerr << "[DEBUG] Error response body: " << response << std::endl;
      }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
  }

  void ChallongeAPI::loadTournament()
  {
    std::string endpoint;
    if (!subdomain.empty())
    {
      endpoint = "/tournaments/" + subdomain + "-" + tournamentUrl + ".json";
    }
    else
    {
      endpoint = "/tournaments/" + tournamentUrl + ".json";
    }

    json params = {{"include_participants", "1"}, {"include_matches", "1"}};

    std::cout << "[DEBUG] Loading tournament from: " << endpoint << std::endl;
    std::string response = makeRequest("GET", endpoint, params);
    std::cout << "[DEBUG] Tournament response length: " << response.length() << " bytes" << std::endl;

    try
    {
      json j = json::parse(response);
      if (j.contains("tournament") && j["tournament"].contains("id"))
      {
        tournamentId = std::to_string(j["tournament"]["id"].get<int>());
        std::cout << "✅ Loaded tournament ID: " << tournamentId << std::endl;

        if (j["tournament"].contains("url"))
        {
          std::string url = j["tournament"]["url"].get<std::string>();
          std::cout << "   Tournament URL: " << url << std::endl;
        }
      }
      else
      {
        std::cerr << "❌ ERROR: Could not find tournament ID in response!" << std::endl;
        std::cerr << "   Full response: " << response << std::endl;
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "❌ Error parsing tournament: " << e.what() << std::endl;
      std::cerr << "   Response was: " << response << std::endl;
    }
  }

  void ChallongeAPI::addParticipant(const std::string &name,
                                    const std::string &steamId, int seed)
  {
    if (tournamentId.empty())
    {
      std::cerr << "Cannot add participant: tournament ID is empty!" << std::endl;
      return;
    }

    std::string endpoint = "/tournaments/" + tournamentId + "/participants.json";
    json data = {
        {"participant", {{"name", name}, {"seed", seed}, {"misc", steamId}}}};

    std::cout << "[DEBUG] Adding participant to tournament " << tournamentId << std::endl;
    std::string response = makeRequest("POST", endpoint, data);

    if (response.empty() || response == "[]" || response.length() < 10)
    {
      std::cerr << "Failed to add participant " << name << " - empty response" << std::endl;
      std::cerr << "   Response: " << response << std::endl;
      return;
    }

    try
    {
      json j = json::parse(response);
      if (j.contains("errors"))
      {
        std::cerr << "Error adding participant " << name << ": " << j["errors"].dump() << std::endl;
        return;
      }
      if (j.contains("participant") && j["participant"].contains("id"))
      {
        std::cout << "Added participant: " << name << " (ID: " << j["participant"]["id"] << ")" << std::endl;
      }
      else
      {
        std::cerr << "Unexpected response when adding " << name << ": " << response << std::endl;
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error parsing add participant response: " << e.what() << std::endl;
      std::cerr << "   Response was: " << response << std::endl;
    }
  }

  void ChallongeAPI::startTournament()
  {
    if (tournamentId.empty())
    {
      std::cerr << "Cannot start tournament: tournament ID is empty!" << std::endl;
      return;
    }

    std::string endpoint = "/tournaments/" + tournamentId + "/start.json";
    json data = json::object();

    std::cout << "[DEBUG] Starting tournament " << tournamentId << std::endl;
    std::string response = makeRequest("POST", endpoint, data);

    if (response.empty() || response.length() < 10)
    {
      std::cerr << "Failed to start tournament - empty response" << std::endl;
      std::cerr << "   Response: " << response << std::endl;
      return;
    }

    try
    {
      json j = json::parse(response);
      if (j.contains("errors"))
      {
        std::cerr << "Error starting tournament: " << j["errors"].dump() << std::endl;
        return;
      }
      if (j.contains("tournament"))
      {
        std::string state = j["tournament"].value("state", "unknown");
        std::cout << "Tournament started, state: " << state << std::endl;
      }
      else
      {
        std::cerr << "Unexpected start tournament response: " << response << std::endl;
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error parsing start tournament response: " << e.what() << std::endl;
      std::cerr << "   Response was: " << response << std::endl;
    }
  }

  void ChallongeAPI::resetTournament()
  {
    if (tournamentId.empty())
    {
      std::cerr << "Cannot reset tournament: tournament ID is empty!" << std::endl;
      return;
    }

    std::cout << "[DEBUG] Resetting tournament " << tournamentId << std::endl;

    std::string participantsEndpoint = "/tournaments/" + tournamentId + "/participants.json";
    std::string participantsResponse = makeRequest("GET", participantsEndpoint, json::object());

    try
    {
      json participantsJson = json::parse(participantsResponse);

      for (auto &p : participantsJson)
      {
        if (p.contains("participant"))
        {
          int participantId = p["participant"]["id"].get<int>();
          std::string deleteEndpoint = "/tournaments/" + tournamentId +
                                       "/participants/" + std::to_string(participantId) + ".json";
          std::cout << "[DEBUG] Deleting participant " << participantId << std::endl;
          makeRequest("DELETE", deleteEndpoint, json::object());
        }
      }

      std::string resetEndpoint = "/tournaments/" + tournamentId + "/reset.json";
      std::cout << "[DEBUG] Resetting tournament state" << std::endl;
      makeRequest("POST", resetEndpoint, json::object());

      std::cout << "Tournament reset complete" << std::endl;
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error resetting tournament: " << e.what() << std::endl;
    }
  }

  std::vector<PendingMatch> ChallongeAPI::getPendingMatches()
  {
    if (tournamentId.empty())
    {
      std::cerr << "❌ Cannot get matches: tournament ID is empty!" << std::endl;
      return {};
    }

    std::string endpoint = "/tournaments/" + tournamentId + "/matches.json";
    json params = {{"state", "open"}};

    std::cout << "[DEBUG] Calling Challonge API: " << endpoint << std::endl;
    std::string response = makeRequest("GET", endpoint, params);
    std::cout << "[DEBUG] Challonge matches response length: " << response.length() << " bytes" << std::endl;

    std::vector<PendingMatch> matches;

    try
    {
      json matchesJson = json::parse(response);
      std::cout << "[DEBUG] Parsed matches JSON, size: " << matchesJson.size() << std::endl;

      std::string participantsEndpoint = "/tournaments/" + tournamentId + "/participants.json";
      std::cout << "[DEBUG] Calling Challonge API: " << participantsEndpoint << std::endl;
      std::string participantsResponse = makeRequest("GET", participantsEndpoint, json::object());
      std::cout << "[DEBUG] Challonge participants response length: " << participantsResponse.length() << " bytes" << std::endl;

      json participantsJson = json::parse(participantsResponse);
      std::cout << "[DEBUG] Parsed participants JSON, size: " << participantsJson.size() << std::endl;

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
          std::cout << "[DEBUG] Participant mapping: ID " << id << " = " << name << " (Steam: " << steamId << ")" << std::endl;
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
            {
              std::cout << "[DEBUG] Skipping completed match" << std::endl;
              continue;
            }

            int p1Id = match["player1_id"].get<int>();
            int p2Id = match["player2_id"].get<int>();

            std::cout << "[DEBUG] Found open match: player " << p1Id << " vs player " << p2Id << std::endl;

            if (idToPlayer.count(p1Id) && idToPlayer.count(p2Id))
            {
              PendingMatch pm;
              pm.player1Name = idToPlayer[p1Id].first;
              pm.player1Id = idToPlayer[p1Id].second;
              pm.player2Name = idToPlayer[p2Id].first;
              pm.player2Id = idToPlayer[p2Id].second;
              matches.push_back(pm);

              std::cout << "[DEBUG] Added pending match: " << pm.player1Name << " vs " << pm.player2Name << std::endl;
            }
            else
            {
              std::cout << "[DEBUG] Could not find player info for match" << std::endl;
            }
          }
        }
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "[DEBUG] Error parsing pending matches: " << e.what() << std::endl;
    }

    std::cout << "[DEBUG] Returning " << matches.size() << " pending matches" << std::endl;
    return matches;
  }

  void ChallongeAPI::reportMatch(const std::string &winnerId, const std::string &loserId)
  {
    if (tournamentId.empty())
    {
      std::cerr << "Cannot report match: tournament ID is empty!" << std::endl;
      return;
    }

    std::string matchesEndpoint = "/tournaments/" + tournamentId + "/matches.json";
    json params = {{"state", "open"}};
    std::string response = makeRequest("GET", matchesEndpoint, params);

    try
    {
      json matchesJson = json::parse(response);

      std::string participantsEndpoint = "/tournaments/" + tournamentId + "/participants.json";
      std::string participantsResponse = makeRequest("GET", participantsEndpoint, json::object());
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

    challonge = std::make_unique<ChallongeAPI>(challongeUser, challongeKey, "", tournamentUrl);
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
    std::cout << "[DEBUG] MGE Plugin Message: " << message << std::endl;

    try
    {
      json j = json::parse(message);

      if (!j.contains("type"))
      {
        std::cout << "[DEBUG] No type field" << std::endl;
        return;
      }

      std::string type = j["type"];
      std::cout << "[DEBUG] Message type: " << type << std::endl;

      if (type == "welcome")
      {
        std::cout << "Connected to MGE plugin: " << j.value("message", "") << std::endl;
        requestArenasFromMGE();
        requestPlayersFromMGE();
      }
      else if (type == "response")
      {
        std::string command = j.value("command", "");
        std::cout << "[DEBUG] Response command: " << command << std::endl;

        if (command == "get_players")
        {
          std::cout << "[DEBUG] Processing get_players response" << std::endl;
          std::cout << "[DEBUG] tournamentActive = " << (tournamentActive ? "true" : "false") << std::endl;

          players.clear();
          steamIdToClientId.clear();
          clientIdToSteamId.clear();

          if (j.contains("players"))
          {
            std::cout << "[DEBUG] Players array found, size: " << j["players"].size() << std::endl;

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
              std::cout << "[DEBUG] Added player: " << player.name << " (ID: " << player.clientId << ", ELO: " << player.elo << ")" << std::endl;
            }
            std::cout << "Received " << players.size() << " players from MGE plugin" << std::endl;

            if (tournamentActive && players.size() > 0)
            {
              std::cout << "[DEBUG] Tournament is active, proceeding to add players to Challonge" << std::endl;
              std::cout << "Starting tournament with " << players.size() << " players" << std::endl;

              int seed = 1;
              for (const auto &player : players)
              {
                std::cout << "Adding player to Challonge: " << player.name << " (ELO: " << player.elo << ", Seed: " << seed << ")" << std::endl;
                challonge->addParticipant(player.name, player.steamId, seed++);
              }

              std::cout << "[DEBUG] All players added, starting Challonge tournament" << std::endl;
              challonge->startTournament();

              std::cout << "[DEBUG] Tournament started, assigning pending matches" << std::endl;
              assignPendingMatches();
            }
            else
            {
              std::cout << "[DEBUG] Not starting - tournamentActive=" << tournamentActive << ", players=" << players.size() << std::endl;
            }
          }
          else
          {
            std::cout << "[DEBUG] No players array in response" << std::endl;
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

    std::cout << "[DEBUG] Fetching pending matches from Challonge..." << std::endl;
    auto pendingMatches = challonge->getPendingMatches();
    std::cout << "[DEBUG] Got " << pendingMatches.size() << " pending matches" << std::endl;

    if (pendingMatches.empty())
    {
      std::cout << "[DEBUG] No pending matches available" << std::endl;
      return;
    }

    for (const auto &match : pendingMatches)
    {
      std::cout << "[DEBUG] Processing match: " << match.player1Name << " vs " << match.player2Name << std::endl;
      std::cout << "[DEBUG] Player 1 ID: " << match.player1Id << ", Player 2 ID: " << match.player2Id << std::endl;

      if (isPlayerInMatch(match.player1Id) || isPlayerInMatch(match.player2Id))
      {
        std::cout << "[DEBUG] One or both players already in a match, skipping" << std::endl;
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

      std::cout << "[DEBUG] Checking if players exist in mapping..." << std::endl;
      std::cout << "[DEBUG] steamIdToClientId has " << steamIdToClientId.size() << " entries" << std::endl;
      std::cout << "[DEBUG] Looking for player1Id: " << match.player1Id << std::endl;
      std::cout << "[DEBUG] Looking for player2Id: " << match.player2Id << std::endl;

      if (steamIdToClientId.count(match.player1Id) && steamIdToClientId.count(match.player2Id))
      {
        int client1 = steamIdToClientId[match.player1Id];
        int client2 = steamIdToClientId[match.player2Id];

        std::cout << "[DEBUG] Found client IDs: " << client1 << " and " << client2 << std::endl;

        addPlayerToMGEArena(client1, arenaId + 1);
        addPlayerToMGEArena(client2, arenaId + 1);

        std::cout << "Assigned match: " << match.player1Name << " vs "
                  << match.player2Name << " to arena " << (arenaId + 1) << std::endl;
      }
      else
      {
        std::cout << "[DEBUG] ERROR: Could not find client IDs for players!" << std::endl;
        std::cout << "[DEBUG] Player 1 (" << match.player1Id << ") exists: " << (steamIdToClientId.count(match.player1Id) ? "YES" : "NO") << std::endl;
        std::cout << "[DEBUG] Player 2 (" << match.player2Id << ") exists: " << (steamIdToClientId.count(match.player2Id) ? "YES" : "NO") << std::endl;
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

    std::cout << "[DEBUG] Resetting tournament..." << std::endl;
    challonge->resetTournament();

    requestPlayersFromMGE();

    std::cout << "Waiting for player list from MGE plugin..." << std::endl;
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