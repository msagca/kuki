#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gesture.hpp>
#include <kuki_engine_export.h>
#include <span>
#include <transport.hpp>
#include <type_traits>
#include <vector>
namespace kuki {
/// @brief Appends values to a snapshot of a game's state, byte for byte.
///
/// Write fields one at a time. A whole struct is copied with its padding, whatever that happens to
/// hold, and the snapshot is also what two machines compare to find out whether they still agree: a
/// difference in bytes nothing reads would be reported as the games having drifted apart.
class StateWriter {
public:
  explicit StateWriter(std::vector<std::byte> &out)
    : out(out) {}
  template <typename T>
    requires std::is_trivially_copyable_v<T>
  auto Write(const T &value) -> void {
    const auto *bytes = reinterpret_cast<const std::byte *>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
  }
private:
  std::vector<std::byte> &out;
};
/// @brief Reads back, in the same order, what a `StateWriter` wrote.
///
/// Running out of bytes does not stop anything -- the values read after it are zero -- and `Ok`
/// says afterwards whether it happened, so the code that loads does not have to check every field.
class StateReader {
public:
  explicit StateReader(const std::span<const std::byte> in)
    : in(in) {}
  template <typename T>
    requires std::is_trivially_copyable_v<T>
  auto Read() -> T {
    T value{};
    if (at + sizeof(T) > in.size()) {
      failed = true;
      return value;
    }
    std::memcpy(&value, in.data() + at, sizeof(T));
    at += sizeof(T);
    return value;
  }
  auto Ok() const -> bool {
    return !failed;
  }
private:
  std::span<const std::byte> in;
  size_t at{};
  bool failed{};
};
/// @brief What a game has to be for its ticks to be taken back and run again.
///
/// Nothing here knows about the scene, the window or the application: a game is a state that moves
/// forward one tick at a time given each player's buttons, can be written down, and can be put back
/// from what was written. The session calls `Advance` for every tick it runs, `Save` before each, and
/// `Load` to go back to one of them when it turns out a tick was run on a guess that was wrong.
///
/// Three things are asked of the implementation, and everything else follows from them.
///
/// Everything that decides what happens next is in the snapshot, and nothing else decides it. A
/// field left out of `Save` is not rolled back, and one that is read from outside -- the clock, the
/// scene, a random device -- is different on the two machines. The scene is for drawing: a game that
/// keeps a figure's place only there has no way to take it back.
///
/// `Advance` gives the same state from the same state and buttons, on both machines. Floating point
/// is fine as long as the two run the same build; `sin` and `cos` of the standard library are not
/// promised to agree across builds, nor is the order of an unordered container.
///
/// `Advance` is cheap, and does not allocate. A rollback runs up to `maxPrediction` of them in the
/// time of one frame, on top of the one the frame owes.
class RollbackGame {
public:
  virtual ~RollbackGame() = default;
  /// @brief Runs one tick. `buttons` is indexed by player, and player 0 is the same player on both
  /// machines.
  ///
  /// `replay` says the tick has been run before and is being run again with different buttons.
  /// Anything that must happen once and be seen -- a sound, a spark, a number on the screen --
  /// should not be done on the strength of a tick that may yet be taken back, which is every tick
  /// until `RollbackSession::GetConfirmedTick` has passed it. Record what happened against the tick
  /// and act on it from there, or throw it away when the tick is run again.
  virtual auto Advance(const int64_t tick, const std::span<const ButtonMask> buttons, const bool replay) -> void = 0;
  /// @brief Appends the whole state to the vector, which is empty. See `StateWriter`.
  virtual auto Save(std::vector<std::byte> &) const -> void = 0;
  /// @brief Puts the state back from what `Save` wrote.
  virtual auto Load(const std::span<const std::byte>) -> void = 0;
};
struct RollbackConfig {
  /// @brief Which of the two players this machine is, 0 or 1. The other machine has the other.
  int localPlayer{};
  /// @brief How many ticks a button is held back before it counts, here as well as at the peer.
  ///
  /// The price of not rolling back more than is needed: an input that reaches the peer before it is
  /// due is one it never has to guess. Sixty ticks a second makes two ticks 33 ms, which is about
  /// the longest delay that goes unfelt, and a link slower than that is covered by prediction.
  int inputDelay{2};
  /// @brief How many ticks may run on a guess about the peer's buttons before this machine stops
  /// and waits. Also how far back a rollback can reach.
  ///
  /// Zero makes it lockstep, which is never wrong and never takes anything back, and is as slow as
  /// the link. Beyond about eight ticks a rollback is more than a frame's worth of ticks to run at
  /// once, and the picture jumps further than the player will forgive.
  int maxPrediction{8};
  /// @brief Every this many ticks the two machines compare a summary of their state, to learn
  /// whether they still agree. Zero turns it off.
  int checksumInterval{30};
  /// @brief How many calls to `Step` may go by with nothing heard from the peer before it is given
  /// up on. Five seconds at sixty a second.
  int timeout{300};
};
enum class RollbackStatus : uint8_t {
  /// A tick was run.
  Ran,
  /// Nothing was: the peer is too far behind to guess at, or too far behind to be heard. Call again.
  Stalled,
  /// The peer has been silent for too long. Nothing will be run again.
  Disconnected
};
struct RollbackStep {
  RollbackStatus status{RollbackStatus::Ran};
  /// How many times the game was advanced by this call, ticks run again after a rollback included.
  int ticks{};
};
struct RollbackStats {
  int rollbacks{};
  int resimulated{};
  /// The most ticks a single rollback took back.
  int deepest{};
  int stalls{};
  /// How many times the two machines' summaries of the same tick have been put side by side. A
  /// match that has agreed throughout and has never compared anything has proved nothing.
  int checksumsCompared{};
};
/// @brief Keeps two copies of a game in step over a link that takes longer than a tick to cross.
///
/// Each machine runs the whole game and sends the other only what its player pressed. Waiting for
/// the other player's buttons before each tick would make every tick as late as the link; instead
/// the missing buttons are guessed -- that nothing has changed -- and the tick is run at once. When
/// the real ones arrive and a guess was wrong, the game is put back to the state before the tick it
/// was wrong on and the ticks since are run again with what is now known. Most of the time the guess
/// is right, because a button stays down for many ticks, and the player sees no difference; when it
/// is wrong the picture shows the correct thing a few ticks late rather than every tick late.
///
/// A button is held back by `inputDelay` ticks on both sides, so most inputs arrive before they are
/// needed and are never guessed at.
///
/// Nothing here knows about the window, the application or a socket. It is stepped by the caller,
/// once for every tick its clock says is due, and speaks to the peer through a `Transport`. That is
/// what lets two of them be run in one test, joined by a `SimulatedLink` that loses and delays
/// datagrams, and checked against a game run with no link at all.
///
/// Each datagram carries every input of the sender's that it has not heard acknowledged, up to a
/// limit, so one that is lost is made up by the next and nothing is ever asked for again.
///
/// Not here yet, and needed before it can be played over the internet: agreeing to begin on the same
/// tick, and on which player is which; and acting on `GetAdvantage` by running the tick clock slow
/// when this machine is ahead.
class KUKI_ENGINE_API RollbackSession {
public:
  /// How many ticks of input are kept, a power of two so that a tick's place is a mask.
  static constexpr int INPUT_RING = 256;
  /// The most ticks `maxPrediction` can be asked to be.
  static constexpr int MAX_PREDICTION = 32;
  RollbackSession(RollbackGame &, Transport &, const RollbackConfig & = {});
  /// @brief Whether a datagram is one of a session's, as opposed to the handshake's or noise.
  ///
  /// For whatever came before the session on the same transport: a peer that has begun is a peer
  /// that has finished the handshake, and this is how to tell without having to be told.
  static auto IsSessionDatagram(const std::span<const std::byte>) -> bool;
  /// @brief Takes in what has arrived, takes back whatever it showed to be wrong, and runs the next
  /// tick if it is safe to.
  ///
  /// Call it once for each tick the clock says is due, with what the local player has pressed. The
  /// buttons are used for the tick `inputDelay` ahead of the one about to run, and a call that
  /// stalls does not use them, so the next call with the same tick should be given fresh ones.
  auto Step(const ButtonMask local) -> RollbackStep;
  /// @brief The number of the tick the next call will run, which is also how many have been.
  auto GetTick() const -> int64_t;
  /// @brief The latest tick that has been run with the real buttons of both players, or -1.
  ///
  /// Everything before and including it is settled and will never be run again.
  auto GetConfirmedTick() const -> int64_t;
  /// @brief How many ticks this machine is ahead of the peer, or behind it if negative.
  ///
  /// The two machines each say how far past the other's last word they are, which includes the time
  /// the word took to arrive; the difference is twice the lead, since the delay is in both. A clock
  /// that stretches its ticks while this is above zero lets the peer catch up, and there is then
  /// less to take back when it does.
  auto GetAdvantage() const -> int;
  /// @brief Whether the two games have been found to differ, and the first tick they were found to
  /// differ at, or -1. The state before that tick is the first that disagreed.
  auto IsDesynced() const -> bool;
  auto GetDesyncTick() const -> int64_t;
  auto IsDisconnected() const -> bool;
  auto GetStats() const -> const RollbackStats &;
  auto GetConfig() const -> const RollbackConfig &;
private:
  struct Checksum {
    int64_t tick{-1};
    uint32_t hash{};
  };
  static constexpr int CHECKSUM_SLOTS = 16;
  RollbackGame &game;
  Transport &transport;
  RollbackConfig config;
  int remote;
  /// What each player pressed, by tick modulo `INPUT_RING`. Local entries are the buttons as
  /// committed, up to `inputDelay` ticks ahead of the game.
  std::array<ButtonMask, INPUT_RING> localInputs{};
  std::array<ButtonMask, INPUT_RING> remoteInputs{};
  /// What the peer was assumed to have pressed on each tick the game has run, to be compared with
  /// what it turns out it did.
  std::array<ButtonMask, INPUT_RING> usedRemote{};
  /// The state before each of the last few ticks, by tick modulo their number.
  std::vector<std::vector<std::byte>> snapshots;
  std::vector<int64_t> snapshotTicks;
  std::array<Checksum, CHECKSUM_SLOTS> mine{};
  std::array<Checksum, CHECKSUM_SLOTS> theirs{};
  std::vector<std::byte> inbox;
  std::vector<std::byte> outbox;
  RollbackStats stats;
  /// The next tick to run.
  int64_t current{};
  /// The latest tick the local player's buttons are committed for.
  int64_t lastLocal{};
  /// The latest tick the peer's buttons are known for, with none missing before it.
  int64_t received{-1};
  /// The latest of this machine's ticks the peer says it has all the buttons up to.
  int64_t peerAck{-1};
  int64_t peerTick{};
  int peerAdvantage{};
  /// The earliest tick a guess turned out wrong on, or -1 if none has.
  int64_t rollbackTo{-1};
  int64_t nextChecksum{};
  int64_t comparedThrough{-1};
  Checksum newest;
  int64_t desyncTick{-1};
  int silence{};
  bool heard{};
  bool disconnected{};
  auto Poll() -> void;
  auto Receive() -> void;
  auto Resimulate() -> int;
  auto UpdateChecksums() -> void;
  auto Compare(const int64_t) -> void;
  auto SendInputs() -> void;
  auto Save(const int64_t) -> void;
  auto Run(const int64_t, const bool) -> void;
  auto InputFor(const int, const int64_t) const -> ButtonMask;
  auto LocalAdvantage() const -> int;
};
} // namespace kuki
