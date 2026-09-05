#include <array>
#include <board.hpp>
#include <cstdlib>
namespace {
constexpr int KNIGHT_OFFSETS[8][2]{{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
constexpr int KING_OFFSETS[8][2]{{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
constexpr int ROOK_DIRECTIONS[4][2]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
constexpr int BISHOP_DIRECTIONS[4][2]{{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
auto ColorIndex(const PieceColor color) -> size_t {
  return color == PieceColor::White ? 0 : 1;
}
/// @brief Which way a colour's pawns advance, in ranks.
auto PawnStep(const PieceColor color) -> int {
  return color == PieceColor::White ? 1 : -1;
}
auto HomeRank(const PieceColor color) -> int {
  return color == PieceColor::White ? 0 : 7;
}
} // namespace
Board::Board() {
  Reset();
}
auto Board::Reset() -> void {
  squares.fill({});
  constexpr PieceType BACK_RANK[Board::Size]{PieceType::Rook, PieceType::Knight, PieceType::Bishop, PieceType::Queen, PieceType::King, PieceType::Bishop, PieceType::Knight, PieceType::Rook};
  for (auto file = 0; file < Size; ++file) {
    squares[SquareOf(file, 0)] = {BACK_RANK[file], PieceColor::White};
    squares[SquareOf(file, 1)] = {PieceType::Pawn, PieceColor::White};
    squares[SquareOf(file, 6)] = {PieceType::Pawn, PieceColor::Black};
    squares[SquareOf(file, 7)] = {BACK_RANK[file], PieceColor::Black};
  }
  toMove = PieceColor::White;
  enPassant = -1;
  kingSideRights = {true, true};
  queenSideRights = {true, true};
}
auto Board::Get(const int square) const -> Piece {
  if (square < 0 || square >= SquareCount)
    return {};
  return squares[square];
}
auto Board::KingSquare(const PieceColor color) const -> int {
  for (auto square = 0; square < SquareCount; ++square)
    if (squares[square].type == PieceType::King && squares[square].color == color)
      return square;
  return -1;
}
auto Board::AttackTargets(const int from, std::vector<int> &out) const -> void {
  const auto piece = squares[from];
  if (!piece.Occupied())
    return;
  const auto file = FileOf(from);
  const auto rank = RankOf(from);
  const auto Slide = [&](const int directions[][2], const int count) {
    for (auto i = 0; i < count; ++i)
      for (auto step = 1; step < Size; ++step) {
        const auto f = file + directions[i][0] * step;
        const auto r = rank + directions[i][1] * step;
        if (!OnBoard(f, r))
          break;
        const auto square = SquareOf(f, r);
        out.push_back(square);
        // A blocked ray still attacks the blocker's square, so this stops after pushing rather
        // than before -- which is what makes a defended piece count as defended.
        if (squares[square].Occupied())
          break;
      }
  };
  switch (piece.type) {
  case PieceType::Pawn: {
    const auto r = rank + PawnStep(piece.color);
    for (const auto f : {file - 1, file + 1})
      if (OnBoard(f, r))
        out.push_back(SquareOf(f, r));
    break;
  }
  case PieceType::Knight:
    for (const auto &offset : KNIGHT_OFFSETS)
      if (OnBoard(file + offset[0], rank + offset[1]))
        out.push_back(SquareOf(file + offset[0], rank + offset[1]));
    break;
  case PieceType::King:
    for (const auto &offset : KING_OFFSETS)
      if (OnBoard(file + offset[0], rank + offset[1]))
        out.push_back(SquareOf(file + offset[0], rank + offset[1]));
    break;
  case PieceType::Bishop:
    Slide(BISHOP_DIRECTIONS, 4);
    break;
  case PieceType::Rook:
    Slide(ROOK_DIRECTIONS, 4);
    break;
  case PieceType::Queen:
    Slide(BISHOP_DIRECTIONS, 4);
    Slide(ROOK_DIRECTIONS, 4);
    break;
  default:
    break;
  }
}
auto Board::Attacked(const int square, const PieceColor by) const -> bool {
  std::vector<int> targets;
  for (auto from = 0; from < SquareCount; ++from) {
    if (!squares[from].Occupied() || squares[from].color != by)
      continue;
    targets.clear();
    AttackTargets(from, targets);
    for (const auto target : targets)
      if (target == square)
        return true;
  }
  return false;
}
auto Board::InCheck(const PieceColor color) const -> bool {
  const auto king = KingSquare(color);
  return king >= 0 && Attacked(king, Opponent(color));
}
auto Board::PseudoMoves(const int from, std::vector<Move> &out) const -> void {
  const auto piece = squares[from];
  if (!piece.Occupied())
    return;
  const auto file = FileOf(from);
  const auto rank = RankOf(from);
  const auto enemy = Opponent(piece.color);
  const auto Push = [&](const int to) {
    if (squares[to].Occupied())
      out.push_back({from, to, to});
    else
      out.push_back({from, to});
  };
  const auto Slide = [&](const int directions[][2], const int count) {
    for (auto i = 0; i < count; ++i)
      for (auto step = 1; step < Size; ++step) {
        const auto f = file + directions[i][0] * step;
        const auto r = rank + directions[i][1] * step;
        if (!OnBoard(f, r))
          break;
        const auto square = SquareOf(f, r);
        if (squares[square].Occupied()) {
          if (squares[square].color == enemy)
            Push(square);
          break;
        }
        Push(square);
      }
  };
  switch (piece.type) {
  case PieceType::Pawn: {
    const auto step = PawnStep(piece.color);
    const auto promotionRank = piece.color == PieceColor::White ? Size - 1 : 0;
    if (const auto r = rank + step; OnBoard(file, r) && !squares[SquareOf(file, r)].Occupied()) {
      Move move{from, SquareOf(file, r)};
      move.promotion = r == promotionRank;
      out.push_back(move);
      // The double push is only available from the home rank and only when both squares are
      // clear, which is why it is nested inside the single push rather than tested on its own.
      const auto startRank = piece.color == PieceColor::White ? 1 : Size - 2;
      if (rank == startRank && !squares[SquareOf(file, r + step)].Occupied())
        out.push_back({from, SquareOf(file, r + step)});
    }
    for (const auto f : {file - 1, file + 1}) {
      const auto r = rank + step;
      if (!OnBoard(f, r))
        continue;
      const auto square = SquareOf(f, r);
      if (squares[square].Occupied() && squares[square].color == enemy) {
        Move move{from, square, square};
        move.promotion = r == promotionRank;
        out.push_back(move);
      } else if (square == enPassant && !squares[square].Occupied())
        // The captured pawn is beside the destination, not on it.
        out.push_back({from, square, SquareOf(f, rank)});
    }
    break;
  }
  case PieceType::Knight:
    for (const auto &offset : KNIGHT_OFFSETS) {
      const auto f = file + offset[0];
      const auto r = rank + offset[1];
      if (!OnBoard(f, r))
        continue;
      const auto square = SquareOf(f, r);
      if (!squares[square].Occupied() || squares[square].color == enemy)
        Push(square);
    }
    break;
  case PieceType::King: {
    for (const auto &offset : KING_OFFSETS) {
      const auto f = file + offset[0];
      const auto r = rank + offset[1];
      if (!OnBoard(f, r))
        continue;
      const auto square = SquareOf(f, r);
      if (!squares[square].Occupied() || squares[square].color == enemy)
        Push(square);
    }
    const auto home = HomeRank(piece.color);
    if (rank != home || file != 4 || Attacked(from, enemy))
      break;
    const auto index = ColorIndex(piece.color);
    // Castling out of, through, or into check is all forbidden. The square the king lands on is
    // covered by the ordinary self-check filter in `LegalMoves`; the one it crosses is not, and
    // is what this tests.
    if (kingSideRights[index] && !squares[SquareOf(5, home)].Occupied() && !squares[SquareOf(6, home)].Occupied() && !Attacked(SquareOf(5, home), enemy)) {
      Move move{from, SquareOf(6, home)};
      move.rookFrom = SquareOf(7, home);
      move.rookTo = SquareOf(5, home);
      out.push_back(move);
    }
    if (queenSideRights[index] && !squares[SquareOf(3, home)].Occupied() && !squares[SquareOf(2, home)].Occupied() && !squares[SquareOf(1, home)].Occupied() && !Attacked(SquareOf(3, home), enemy)) {
      Move move{from, SquareOf(2, home)};
      move.rookFrom = SquareOf(0, home);
      move.rookTo = SquareOf(3, home);
      out.push_back(move);
    }
    break;
  }
  case PieceType::Bishop:
    Slide(BISHOP_DIRECTIONS, 4);
    break;
  case PieceType::Rook:
    Slide(ROOK_DIRECTIONS, 4);
    break;
  case PieceType::Queen:
    Slide(BISHOP_DIRECTIONS, 4);
    Slide(ROOK_DIRECTIONS, 4);
    break;
  default:
    break;
  }
}
auto Board::SelfCheck(const Move &move) const -> bool {
  // Played on a copy rather than made and unmade. A board is 64 bytes of plain data and this runs
  // on a click, so the copy costs nothing worth the bugs an undo path would invite.
  Board trial(*this);
  const auto mover = squares[move.from].color;
  trial.Apply(move);
  return trial.InCheck(mover);
}
auto Board::LegalMoves(const int from) const -> std::vector<Move> {
  std::vector<Move> moves;
  if (from < 0 || from >= SquareCount)
    return moves;
  const auto piece = squares[from];
  if (!piece.Occupied() || piece.color != toMove)
    return moves;
  std::vector<Move> pseudo;
  PseudoMoves(from, pseudo);
  for (const auto &move : pseudo)
    if (!SelfCheck(move))
      moves.push_back(move);
  return moves;
}
auto Board::Legal(const int from, const int to) const -> std::optional<Move> {
  for (const auto &move : LegalMoves(from))
    if (move.to == to)
      return move;
  return std::nullopt;
}
auto Board::Apply(const Move &move) -> void {
  if (move.from < 0 || move.from >= SquareCount || move.to < 0 || move.to >= SquareCount)
    return;
  const auto piece = squares[move.from];
  if (move.captured >= 0 && move.captured < SquareCount)
    squares[move.captured] = {};
  squares[move.from] = {};
  squares[move.to] = piece;
  if (move.promotion)
    squares[move.to].type = PieceType::Queen;
  if (move.Castle()) {
    squares[move.rookTo] = squares[move.rookFrom];
    squares[move.rookFrom] = {};
  }
  // Rights are lost by the king or the rook moving, and by the rook being captured on its own
  // square -- the last of which is easy to forget and shows up as castling with a rook that is no
  // longer there.
  const auto index = ColorIndex(piece.color);
  if (piece.type == PieceType::King) {
    kingSideRights[index] = false;
    queenSideRights[index] = false;
  }
  const auto ClearRookRight = [&](const int square) {
    for (const auto color : {PieceColor::White, PieceColor::Black}) {
      const auto home = HomeRank(color);
      if (square == SquareOf(0, home))
        queenSideRights[ColorIndex(color)] = false;
      else if (square == SquareOf(7, home))
        kingSideRights[ColorIndex(color)] = false;
    }
  };
  ClearRookRight(move.from);
  ClearRookRight(move.to);
  enPassant = -1;
  if (piece.type == PieceType::Pawn && std::abs(RankOf(move.to) - RankOf(move.from)) == 2)
    enPassant = SquareOf(FileOf(move.from), (RankOf(move.from) + RankOf(move.to)) / 2);
  toMove = Opponent(toMove);
}
auto Board::AnyLegalMove(const PieceColor color) const -> bool {
  for (auto square = 0; square < SquareCount; ++square) {
    if (!squares[square].Occupied() || squares[square].color != color)
      continue;
    std::vector<Move> pseudo;
    PseudoMoves(square, pseudo);
    for (const auto &move : pseudo)
      if (!SelfCheck(move))
        return true;
  }
  return false;
}
auto Board::State() const -> GameState {
  const auto check = InCheck(toMove);
  if (AnyLegalMove(toMove))
    return check ? GameState::Check : GameState::Playing;
  return check ? GameState::Checkmate : GameState::Stalemate;
}
