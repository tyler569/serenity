/*
 * Copyright (c) 2020, the SerenityOS developers.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Chess.h"
#include <AK/Assertions.h>
#include <AK/LogStream.h>
#include <AK/Vector.h>
#include <stdlib.h>

Chess::Square::Square(const StringView& name)
{
    ASSERT(name.length() == 2);
    char filec = name[0];
    char rankc = name[1];

    if (filec >= 'a' && filec <= 'h') {
        file = filec - 'a';
    } else if (filec >= 'A' && filec <= 'H') {
        file = filec - 'A';
    } else {
        ASSERT_NOT_REACHED();
    }

    if (rankc >= '1' && rankc <= '8') {
        rank = rankc - '1';
    } else {
        ASSERT_NOT_REACHED();
    }
}

Chess::Chess()
{
    // Fill empty spaces.
    for (unsigned rank = 2; rank < 6; ++rank) {
        for (unsigned file = 0; file < 8; ++file) {
            set_piece({ rank, file }, EmptyPiece);
        }
    }

    // Fill white pawns.
    for (unsigned file = 0; file < 8; ++file) {
        set_piece({ 1, file }, { Colour::White, Type::Pawn });
    }

    // Fill black pawns.
    for (unsigned file = 0; file < 8; ++file) {
        set_piece({ 6, file }, { Colour::Black, Type::Pawn });
    }

    // Fill while pieces.
    set_piece(Square("a1"), { Colour::White, Type::Rook });
    set_piece(Square("b1"), { Colour::White, Type::Knight });
    set_piece(Square("c1"), { Colour::White, Type::Bishop });
    set_piece(Square("d1"), { Colour::White, Type::Queen });
    set_piece(Square("e1"), { Colour::White, Type::King });
    set_piece(Square("f1"), { Colour::White, Type::Bishop });
    set_piece(Square("g1"), { Colour::White, Type::Knight });
    set_piece(Square("h1"), { Colour::White, Type::Rook });

    // Fill black pieces.
    set_piece(Square("a8"), { Colour::Black, Type::Rook });
    set_piece(Square("b8"), { Colour::Black, Type::Knight });
    set_piece(Square("c8"), { Colour::Black, Type::Bishop });
    set_piece(Square("d8"), { Colour::Black, Type::Queen });
    set_piece(Square("e8"), { Colour::Black, Type::King });
    set_piece(Square("f8"), { Colour::Black, Type::Bishop });
    set_piece(Square("g8"), { Colour::Black, Type::Knight });
    set_piece(Square("h8"), { Colour::Black, Type::Rook });
}

Chess::Piece Chess::get_piece(const Square& square) const
{
    ASSERT(square.rank < 8);
    ASSERT(square.file < 8);
    return m_board[square.rank][square.file];
}

Chess::Piece Chess::set_piece(const Square& square, const Piece& piece)
{
    ASSERT(square.rank < 8);
    ASSERT(square.file < 8);
    return m_board[square.rank][square.file] = piece;
}

bool Chess::is_legal(const Move& move, Colour colour) const
{
    if (colour == Colour::None)
        colour = turn();

    if (!is_legal_no_check(move, colour))
        return false;

    Chess clone = *this;
    clone.apply_illegal_move(move, colour);
    if (clone.in_check(colour))
        return false;

    // Don't allow castling through check or out of check.
    Vector<Square> check_squares;
    if (colour == Colour::White && move.from == Square("e1") && get_piece(Square("e1")) == Piece(Colour::White, Type::King)) {
        if (move.to == Square("a1") || move.to == Square("c1")) {
            check_squares = { Square("e1"), Square("d1"), Square("c1") };
        } else if (move.to == Square("h1") || move.to == Square("g1")) {
            check_squares = { Square("e1"), Square("f1"), Square("g1") };
        }
    } else if (colour == Colour::Black && move.from == Square("e8") && get_piece(Square("e8")) == Piece(Colour::Black, Type::King)) {
        if (move.to == Square("a8") || move.to == Square("c8")) {
            check_squares = { Square("e8"), Square("d8"), Square("c8") };
        } else if (move.to == Square("h8") || move.to == Square("g8")) {
            check_squares = { Square("e8"), Square("f8"), Square("g8") };
        }
    }
    for (auto& square : check_squares) {
        Chess clone = *this;
        clone.set_piece(move.from, EmptyPiece);
        clone.set_piece(square, { colour, Type::King });
        if (clone.in_check(colour))
            return false;
    }

    return true;
}

bool Chess::is_legal_no_check(const Move& move, Colour colour) const
{
    auto piece = get_piece(move.from);
    if (piece.colour != colour)
        return false;

    if (move.to.rank > 7 || move.to.file > 7)
        return false;

    if (piece.type == Type::Pawn) {
        // FIXME: Add en passant.
        if (colour == Colour::White) {
            if (move.to.rank == move.from.rank + 1 && move.to.file == move.from.file && get_piece(move.to).type == Type::None) {
                // Regular pawn move.
                return true;
            } else if (move.to.rank == move.from.rank + 1 && (move.to.file == move.from.file + 1 || move.to.file == move.from.file - 1)
                && get_piece(move.to).colour == Colour::Black) {
                // Pawn capture.
                return true;
            } else if (move.from.rank == 1 && move.to.rank == move.from.rank + 2 && move.to.file == move.from.file && get_piece(move.to).type == Type::None) {
                // 2 square pawn move from initial position.
                return true;
            }
        } else if (colour == Colour::Black) {
            if (move.to.rank == move.from.rank - 1 && move.to.file == move.from.file && get_piece(move.to).type == Type::None) {
                // Regular pawn move.
                return true;
            } else if (move.to.rank == move.from.rank - 1 && (move.to.file == move.from.file + 1 || move.to.file == move.from.file - 1)
                && get_piece(move.to).colour == Colour::White) {
                // Pawn capture.
                return true;
            } else if (move.from.rank == 6 && move.to.rank == move.from.rank - 2 && move.to.file == move.from.file && get_piece(move.to).type == Type::None) {
                // 2 square pawn move from initial position.
                return true;
            }
        }
        return false;
    } else if (piece.type == Type::Knight) {
        int rank_delta = abs(move.to.rank - move.from.rank);
        int file_delta = abs(move.to.file - move.from.file);
        if (get_piece(move.to).colour != colour && max(rank_delta, file_delta) == 2 && min(rank_delta, file_delta) == 1) {
            return true;
        }
    } else if (piece.type == Type::Bishop) {
        int rank_delta = move.to.rank - move.from.rank;
        int file_delta = move.to.file - move.from.file;
        if (abs(rank_delta) == abs(file_delta)) {
            int dr = rank_delta / abs(rank_delta);
            int df = file_delta / abs(file_delta);
            for (Square sq = move.from; sq != move.to; sq.rank += dr, sq.file += df) {
                if (get_piece(sq).type != Type::None && sq != move.from) {
                    return false;
                }
            }

            if (get_piece(move.to).colour != colour) {
                return true;
            }
        }
    } else if (piece.type == Type::Rook) {
        int rank_delta = move.to.rank - move.from.rank;
        int file_delta = move.to.file - move.from.file;
        if (rank_delta == 0 || file_delta == 0) {
            int dr = (rank_delta) ? rank_delta / abs(rank_delta) : 0;
            int df = (file_delta) ? file_delta / abs(file_delta) : 0;
            for (Square sq = move.from; sq != move.to; sq.rank += dr, sq.file += df) {
                if (get_piece(sq).type != Type::None && sq != move.from) {
                    return false;
                }
            }

            if (get_piece(move.to).colour != colour) {
                return true;
            }
        }
    } else if (piece.type == Type::Queen) {
        int rank_delta = move.to.rank - move.from.rank;
        int file_delta = move.to.file - move.from.file;
        if (abs(rank_delta) == abs(file_delta) || rank_delta == 0 || file_delta == 0) {
            int dr = (rank_delta) ? rank_delta / abs(rank_delta) : 0;
            int df = (file_delta) ? file_delta / abs(file_delta) : 0;
            for (Square sq = move.from; sq != move.to; sq.rank += dr, sq.file += df) {
                if (get_piece(sq).type != Type::None && sq != move.from) {
                    return false;
                }
            }

            if (get_piece(move.to).colour != colour) {
                return true;
            }
        }
    } else if (piece.type == Type::King) {
        int rank_delta = move.to.rank - move.from.rank;
        int file_delta = move.to.file - move.from.file;
        if (abs(rank_delta) <= 1 && abs(file_delta) <= 1) {
            if (get_piece(move.to).colour != colour) {
                return true;
            }
        }

        if (colour == Colour::White) {
            if ((move.to == Square("a1") || move.to == Square("c1")) && m_white_can_castle_queenside && get_piece(Square("b1")).type == Type::None && get_piece(Square("c1")).type == Type::None && get_piece(Square("d1")).type == Type::None) {

                return true;
            } else if ((move.to == Square("h1") || move.to == Square("g1")) && m_white_can_castle_kingside && get_piece(Square("f1")).type == Type::None && get_piece(Square("g1")).type == Type::None) {
                return true;
            }
        } else {
            if ((move.to == Square("a8") || move.to == Square("c8")) && m_black_can_castle_queenside && get_piece(Square("b8")).type == Type::None && get_piece(Square("c8")).type == Type::None && get_piece(Square("d8")).type == Type::None) {

                return true;
            } else if ((move.to == Square("h8") || move.to == Square("g8")) && m_black_can_castle_kingside && get_piece(Square("f8")).type == Type::None && get_piece(Square("g8")).type == Type::None) {
                return true;
            }
        }
    }

    return false;
}

bool Chess::in_check(Colour colour) const
{
    Square king_square = { 50, 50 };
    Square::for_each([&](const Square& square) {
        if (get_piece(square) == Piece(colour, Type::King)) {
            king_square = square;
            return IterationDecision::Break;
        }
        return IterationDecision::Continue;
    });

    bool check = false;
    Square::for_each([&](const Square& square) {
        if (is_legal({ square, king_square }, opposing_colour(colour))) {
            check = true;
            return IterationDecision::Break;
        } else if (get_piece(square) == Piece(opposing_colour(colour), Type::King) && is_legal_no_check({ square, king_square }, opposing_colour(colour))) {
            // The King is a special case, because it would be in check if it put the opposing king in check.
            check = true;
            return IterationDecision::Break;
        }
        return IterationDecision::Continue;
    });

    return check;
}

bool Chess::apply_move(const Move& move, Colour colour)
{
    if (colour == Colour::None)
        colour = turn();

    if (!is_legal(move, colour))
        return false;

    return apply_illegal_move(move, colour);
}

bool Chess::apply_illegal_move(const Move& move, Colour colour)
{
    m_turn = opposing_colour(colour);

    // FIXME: pawn promotion

    if (move.from == Square("a1") || move.to == Square("a1") || move.from == Square("e1"))
        m_white_can_castle_queenside = false;
    if (move.from == Square("h1") || move.to == Square("h1") || move.from == Square("e1"))
        m_white_can_castle_kingside = false;
    if (move.from == Square("a8") || move.to == Square("a8") || move.from == Square("e8"))
        m_black_can_castle_queenside = false;
    if (move.from == Square("h8") || move.to == Square("h8") || move.from == Square("e8"))
        m_black_can_castle_kingside = false;

    if (colour == Colour::White && move.from == Square("e1") && get_piece(Square("e1")) == Piece(Colour::White, Type::King)) {
        if (move.to == Square("a1") || move.to == Square("c1")) {
            set_piece(Square("e1"), EmptyPiece);
            set_piece(Square("a1"), EmptyPiece);
            set_piece(Square("c1"), { Colour::White, Type::King });
            set_piece(Square("d1"), { Colour::White, Type::Rook });
            return true;
        } else if (move.to == Square("h1") || move.to == Square("g1")) {
            set_piece(Square("e1"), EmptyPiece);
            set_piece(Square("h1"), EmptyPiece);
            set_piece(Square("g1"), { Colour::White, Type::King });
            set_piece(Square("f1"), { Colour::White, Type::Rook });
            return true;
        }
    } else if (colour == Colour::Black && move.from == Square("e8") && get_piece(Square("e8")) == Piece(Colour::Black, Type::King)) {
        if (move.to == Square("a8") || move.to == Square("c8")) {
            set_piece(Square("e8"), EmptyPiece);
            set_piece(Square("a8"), EmptyPiece);
            set_piece(Square("c8"), { Colour::White, Type::King });
            set_piece(Square("d8"), { Colour::White, Type::Rook });
            return true;
        } else if (move.to == Square("h8") || move.to == Square("g8")) {
            set_piece(Square("e8"), EmptyPiece);
            set_piece(Square("h8"), EmptyPiece);
            set_piece(Square("g8"), { Colour::Black, Type::King });
            set_piece(Square("f8"), { Colour::Black, Type::Rook });
            return true;
        }
    }

    set_piece(move.to, get_piece(move.from));
    set_piece(move.from, EmptyPiece);

    return true;
}
