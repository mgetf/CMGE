#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <set>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct lws;
struct lws_context;

namespace mge
{

  struct Player
  {
    std::string steamId;
    std::string name;
    int elo;

    json toJson() const
    {
      return {{"steamId", steamId}, {"name", name}, {"elo", elo}};
    }
  };

  struct Arena
  {
    std::optional<std::set<std::string>> currentMatch;

    void clear() { currentMatch.reset(); }
    bool isEmpty() const { return !currentMatch.has_value(); }
    bool hasPlayer(const std::string &steamId) const
    {
      if (!currentMatch)
        return false;
      return currentMatch->count(steamId) > 0;
    }
  };

  struct PendingMatch
  {
    std::string player1Name;
    std::string player1Id;
    std::string player2Name;
    std::string player2Id;
  };

  struct WebSocketConnection
  {
    lws *wsi;
    std::string type; // "server" or "admin"
    std::vector<std::string> messageQueue;
  };

  class ChallongeAPI
  {
  private:
    std::string username;
    std::string apiKey;
    std::string subdomain;
    std::string tournamentUrl;
    std::string tournamentId;

    std::string makeRequest(const std::string &method,
                            const std::string &endpoint,
                            const json &data = json::object());

  public:
    ChallongeAPI(const std::string &user, const std::string &key,
                 const std::string &subdomain, const std::string &tournamentUrl);

    void loadTournament();
    void addParticipant(const std::string &name, const std::string &steamId, int seed);
    void startTournament();
    std::vector<PendingMatch> getPendingMatches();
    void reportMatch(const std::string &winnerId, const std::string &loserId);

    const std::string &getTournamentId() const { return tournamentId; }
  };

  class TournamentManager
  {
  private:
    static constexpr int NUM_ARENAS = 16;

    std::vector<Arena> arenas;
    std::vector<int> arenaPriority;
    std::vector<Player> players;
    std::map<lws *, std::unique_ptr<WebSocketConnection>> connections;
    WebSocketConnection *admin = nullptr;

    std::unique_ptr<ChallongeAPI> challonge;
    lws_context *context;

    std::optional<int> getOpenArena();
    void assignPendingMatches();
    bool isPlayerInMatch(const std::string &steamId) const;
    void broadcastToServers(const json &message);
    void sendToConnection(WebSocketConnection *conn, const json &message);

  public:
    TournamentManager(lws_context *ctx, const std::string &challongeUser,
                      const std::string &challongeKey, const std::string &tournamentUrl);

    void handleMessage(lws *wsi, const std::string &message);
    void addConnection(lws *wsi);
    void removeConnection(lws *wsi);
    void queueMessage(lws *wsi, const std::string &message);
    bool hasQueuedMessages(lws *wsi) const;
    std::string popMessage(lws *wsi);

    void handleServerHello(lws *wsi, const json &payload);
    void handleTournamentStart(const json &payload);
    void handleTournamentStop(const json &payload);
    void handleUsersInServer(const json &payload);
    void handleMatchResults(const json &payload);
    void handleMatchBegan(const json &payload);
    void handleMatchDetails(const json &payload);
    void handleSetMatchScore(lws *wsi, const json &payload);
    void handleMatchCancel(const json &payload);
  };

}