#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
/// @brief The rules, with no dependency on the engine at all.
///
/// Deliberately separate from everything that draws or clicks: the board knows nothing about
/// entities, and `GameManager` owns the translation between the two. That split is what makes the
/// rules testable without a window, and it is the one piece of a game that benefits most from it.
enum class PieceType : uint8_t { None,
  Pawn,
  Knight,
  Bishop,
  Rook,
  Queen,
  King };
enum class PieceColor : uint8_t { White,
  Black };
/// @brief The position's standing, from the point of view of the side to move.
enum class GameState : uint8_t { Playing,
  Check,
  Checkmate,
  Stalemate };
struct Piece {
  PieceType type{PieceType::None};
  PieceColor color{PieceColor::White};
  auto Occupied() const -> bool {
    return type != PieceType::None;
  }
};
/// @brief One move, carrying everything `Apply` needs so that nothing has to be re-derived.
///
/// `captured` is a square rather than a flag because en passant takes a piece that is not standing
/// on the destination, and a caller removing the piece at `to` would leave the pawn behind.
struct Move {
  int from{-1};
  int to{-1};
  int captured{-1};
  int rookFrom{-1};
  int rookTo{-1};
  bool promotion{};
  auto Castle() const -> bool {
    return rookFrom >= 0;
  }
};
/// @brief A chess position and the moves legal in it.
///
/// Complete except for the draws nobody notices in a two-player game at a desk: there is no
/// threefold repetition, no fifty-move rule, and promotion is always to a queen. Castling and en
/// passant are both here, since a game missing those reads as broken rather than as simplified.
class Board {
public:
  static constexpr int Size = 8;
  static constexpr int SquareCount = Size * Size;
  Board();
  /// @brief Puts the pieces back and gives white the move.
  auto Reset() -> void;
  auto Get(const int) const -> Piece;
  auto ToMove() const -> PieceColor {
    return toMove;
  }
  /// @brief Standing of the side to move, which is what a status line wants to report.
  auto State() const -> GameState;
  /// @brief Every legal move for the piece on this square. Empty when it is not that side's turn.
  auto LegalMoves(const int) const -> std::vector<Move>;
  /// @brief The legal move between two squares, if there is one.
  auto Legal(const int, const int) const -> std::optional<Move>;
  auto Apply(const Move &) -> void;
  auto InCheck(const PieceColor) const -> bool;
  static auto FileOf(const int square) -> int {
    return square % Size;
  }
  static auto RankOf(const int square) -> int {
    return square / Size;
  }
  static auto SquareOf(const int file, const int rank) -> int {
    return rank * Size + file;
  }
  static auto OnBoard(const int file, const int rank) -> bool {
    return file >= 0 && file < Size && rank >= 0 && rank < Size;
  }
  static auto Opponent(const PieceColor color) -> PieceColor {
    return color == PieceColor::White ? PieceColor::Black : PieceColor::White;
  }
private:
  std::array<Piece, SquareCount> squares{};
  PieceColor toMove{PieceColor::White};
  /// @brief Square a pawn may be captured on this turn by moving onto it, or -1.
  int enPassant{-1};
  std::array<bool, 2> kingSideRights{};
  std::array<bool, 2> queenSideRights{};
  /// @brief Moves ignoring whether they leave the mover's own king in check.
  auto PseudoMoves(const int, std::vector<Move> &) const -> void;
  /// @brief Squares a piece bears on, used to answer "is this square attacked".
  ///
  /// Separate from `PseudoMoves` and not merely a filter of it, for two reasons that both matter:
  /// a pawn attacks diagonally but moves straight ahead, and castling asks whether squares are
  /// attacked -- so generating it here as well would recurse without end.
  auto AttackTargets(const int, std::vector<int> &) const -> void;
  auto Attacked(const int, const PieceColor) const -> bool;
  auto KingSquare(const PieceColor) const -> int;
  /// @brief Whether making this move would leave the mover's own king attacked.
  auto SelfCheck(const Move &) const -> bool;
  auto AnyLegalMove(const PieceColor) const -> bool;
};
