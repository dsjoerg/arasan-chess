// Copyright 1994-2012 by Jon Dart.  All Rights Reserved.

#include "constant.h"
#include "chess.h"
#include "board.h"
#include "util.h"
#include "attacks.h"
#include "debug.h"
#include "boardio.h"
#include "bhash.h"
#include <ctype.h>
#include <memory.h>
#include <assert.h>
#include <iostream>

using namespace std;

const Bitboard Board::black_squares(0xaa55aa55aa55aa55ULL);

const Bitboard Board::white_squares(0x55aa55aa55aa55aaULL);

const hash_t rep_codes[3] =
{
    0x194ca2c45c8e7baaULL,
    0x804e48e8e8f5544fULL,
    0xd4767986f0ab49a7ULL
};

static Board *initialBoard = NULL;

void Board::setupInitialBoard() {
   initialBoard = (Board*)malloc(sizeof(Board));
   static PieceType pieces[] =
   {
      Rook,
      Knight,
      Bishop,
      Queen,
      King,
      Bishop,
      Knight,
      Rook
   };

   initialBoard->side = White;
   initialBoard->state.checkStatus = CheckUnknown;
   for (int i=0;i<64;i++)
   {
      const Square sq(i);
      if (Rank<White>(sq) == 1)
         initialBoard->contents[sq] = MakeWhitePiece( pieces[File(sq)-1]);
      else if (Rank<Black>(sq) == 1)
         initialBoard->contents[sq] = MakeBlackPiece( pieces[File(sq)-1]);
      else if (Rank<White>(sq) == 2)
         initialBoard->contents[sq] = WhitePawn;
      else if (Rank<Black>(sq) == 2)
         initialBoard->contents[sq] = BlackPawn;
      else
         initialBoard->contents[sq] = EmptyPiece;
   }
   initialBoard->state.enPassantSq = InvalidSquare;
   initialBoard->state.castleStatus[White] = initialBoard->state.castleStatus[Black] = CanCastleEitherSide;
   initialBoard->state.moveCount = 0;
   initialBoard->repListHead = initialBoard->repList;
   initialBoard->setSecondaryVars();
   *(initialBoard->repListHead)++ = initialBoard->hashCode();
}

void Board::setSecondaryVars()
{
   int i;

   material[White].clear();
   material[Black].clear();
   pawn_bits[White].clear();
   pawn_bits[Black].clear();
   knight_bits[White].clear();
   knight_bits[Black].clear();
   bishop_bits[White].clear();
   bishop_bits[Black].clear();
   rook_bits[White].clear();
   rook_bits[Black].clear();
   queen_bits[White].clear();
   queen_bits[Black].clear();
   occupied[White].clear();
   occupied[Black].clear();
   allOccupied.clear();
   for (i=0;i<64;i++)
   {
      Square sq(i);
      if (contents[sq] != EmptyPiece)
      {
         const Piece piece = contents[sq];
         ColorType color = PieceColor(piece);
         occupied[color].set(sq);
         allOccupied.set(sq);
         material[color].addPiece(TypeOfPiece(piece));
         switch (TypeOfPiece(piece))
         {
         case King:
            kingPos[color] = sq;
            break;
         case Pawn:
            pawn_bits[color].set(sq);
            break;
         case Knight:
            knight_bits[color].set(sq);
            break;
         case Bishop:
            bishop_bits[color].set(sq);
            break;
         case Rook:
            rook_bits[color].set(sq);
            break;
         case Queen:
            queen_bits[color].set(sq);
            break;
         default:
            break;
         }
      }
   }
   state.hashCode = BoardHash::hashCode(*this);
   pawnHashCodeW = BoardHash::pawnHash(*this,White);
   pawnHashCodeB = BoardHash::pawnHash(*this,Black);
}

void Board::reset()
{
   if (!initialBoard) {
       setupInitialBoard();
   }
   *this = *initialBoard;
}

void Board::makeEmpty() {
   for (Square sq = 0; sq < 64; sq++) contents[sq] = EmptyPiece;
   state.castleStatus[White] = state.castleStatus[Black] = CantCastleEitherSide;
}

Board::Board()
{
   reset();
}

Board::Board(const Board &b)
{
   // Copy all contents except the repetition list
   memcpy(&contents,&b.contents,(byte*)repList-(byte*)&contents);
   // Copy the repetition table
   int rep_entries = (int)(b.repListHead - b.repList);
   if (rep_entries) {
     memcpy(repList,b.repList,sizeof(hash_t)*rep_entries);
   }
   repListHead = repList + rep_entries;
}

Board &Board::operator = (const Board &b)
{
   if (&b != this)
   {
      // Copy all contents except the repetition list
      memcpy(&contents,&b.contents,(byte*)repList-(byte*)&contents);
      // Copy the repetition table
      int rep_entries = (int)(b.repListHead - b.repList);
      if (rep_entries) {
          memcpy(repList,b.repList,sizeof(hash_t)*rep_entries);
      }
      repListHead = repList + rep_entries;
   }
   return *this;
}

Board::~Board()
{
}

#ifdef _DEBUG
const Piece &Board::operator[]( const Square sq ) const
{
   ASSERT(OnBoard(sq));
   return contents[sq];
}
#endif

static inline CastleType UpdateCastleStatusW( CastleType cs, Square sq )
// after a move of or capture of the rook on 'sq', update castle status
// for 'side'
{
   ASSERT(cs<3);
   if (sq == A1) // Queen Rook moved or captured
   {
      if (cs == CanCastleEitherSide)
         return CanCastleKSide;
      else if (cs == CanCastleQSide)
         return CantCastleEitherSide;
   }
   else if (sq == H1) // King Rook moved or captured
   {
      if (cs == CanCastleEitherSide)
         return CanCastleQSide;
      else if (cs == CanCastleKSide)
         return CantCastleEitherSide;
   }
   return cs;
}

static inline CastleType UpdateCastleStatusB(CastleType cs, Square sq)
// after a move of or capture of the rook on 'sq', update castle status
// for 'side'
{
   ASSERT(cs<3);
   if (sq == A8) // Queen Rook moved or captured
   {
      if (cs == CanCastleEitherSide)
         return CanCastleKSide;
      else if (cs == CanCastleQSide)
         return CantCastleEitherSide;
   }
   else if (sq==H8) // King Rook moved or captured
   {
      if (cs == CanCastleEitherSide)
         return CanCastleQSide;
      else if (cs == CanCastleKSide)
         return CantCastleEitherSide;
   }
   return cs;
}

#define Xor(h,sq,piece) h ^= hash_codes[sq][(int)piece]

void Board::doNull()
{
   state.checkStatus = CheckUnknown;
   state.moveCount++;
   if (state.enPassantSq != InvalidSquare)
   {
       state.hashCode ^= ep_codes[state.enPassantSq];
       state.hashCode ^= ep_codes[0];
   }
   state.enPassantSq = InvalidSquare;
   side = oppositeSide();
   if (sideToMove() == Black)
       state.hashCode |= (hash_t)1;
   else
       state.hashCode &= (hash_t)~1;
   *repListHead++ = state.hashCode;
   ASSERT(repListHead-repList < (int)RepListSize);
   ASSERT(state.hashCode == BoardHash::hashCode(*this));
}

void Board::doMove( Move move )
{
   ASSERT(!IsNull(move));
   state.checkStatus = CheckUnknown;
   ++state.moveCount;
   if (state.enPassantSq != InvalidSquare)
   {
       state.hashCode ^= ep_codes[state.enPassantSq];
       state.hashCode ^= ep_codes[0];
   }
   Square old_epsq = state.enPassantSq;
   state.enPassantSq = InvalidSquare;

   ASSERT(PieceMoved(move) != Empty);

   const Square start = StartSquare(move);
   const Square dest = DestSquare(move);
   const MoveType moveType = TypeOfMove(move);
   ASSERT(PieceMoved(move) == TypeOfPiece(contents[start]));
#ifdef _DEBUG
   if (Capture(move) != Empty) {
           if (TypeOfMove(move) == EnPassant) {
                   ASSERT(contents[old_epsq] == MakePiece(Pawn,OppositeColor(side)));
           } else {
                   ASSERT(contents[dest] == MakePiece(Capture(move),OppositeColor(side)));
           }
   }
#endif
   if (side == White)
   {
      if (moveType == KCastle)
      {
         state.moveCount = 0;

         // update the hash code
         const Square kp = kingSquare(White);
         Xor(state.hashCode, kp+3, WhiteRook);
         Xor(state.hashCode, kp, WhiteKing);
         Xor(state.hashCode, kp+1, WhiteRook);
         Xor(state.hashCode, kp+2, WhiteKing);
         state.hashCode ^= w_castle_status[(int)state.castleStatus[White]];
         state.hashCode ^= w_castle_status[(int)CastledKSide];

         const int newkp = kp + 2;
         kingPos[White] = newkp;
         state.castleStatus[White] = CastledKSide;
         // find old square of rook
         Square oldrooksq = kp + 3;
         Square newrooksq = kp + 1;
         contents[kp] = contents[oldrooksq] = EmptyPiece;
         contents[newrooksq] = WhiteRook;
         contents[newkp] = WhiteKing;
         rook_bits[White].clear(oldrooksq);
         rook_bits[White].set(newrooksq);
         clearAll(White,kp);
         clearAll(White,oldrooksq);
         setAll(White,newkp);
         setAll(White,newrooksq);
      }
      else if (moveType == QCastle)
      {
         state.moveCount = 0;

         // update the hash code
         const Square kp = kingSquare(White);
         Xor(state.hashCode, kp-4, WhiteRook);
         Xor(state.hashCode, kp, WhiteKing);
         Xor(state.hashCode, kp-1, WhiteRook);
         Xor(state.hashCode, kp-2, WhiteKing);
         state.hashCode ^= w_castle_status[(int)state.castleStatus[White]];
         state.hashCode ^= w_castle_status[(int)CastledQSide];

         const int newkp = kp - 2;
         kingPos[White] = newkp;
         state.castleStatus[White] = CastledQSide;
         // find old square of rook
         Square oldrooksq = kp - 4;
         Square newrooksq = kp - 1;
         contents[kp] = contents[oldrooksq] = Piece();
         contents[newrooksq] = WhiteRook;
         contents[kp-2] = WhiteKing;
         rook_bits[White].clear(oldrooksq);
         rook_bits[White].set(newrooksq);
         clearAll(White,kp);
         clearAll(White,oldrooksq);
         setAll(White,newkp);
         setAll(White,newrooksq);
      }
      else // not castling
      {
         ASSERT(contents[start] != EmptyPiece);
         const Bitboard bits(Bitboard::mask[start] |
                           Bitboard::mask[dest]);
         Square target = dest; // where we captured
                 Piece capture = contents[dest]; // what we captured
         switch (TypeOfPiece(contents[StartSquare(move)])) {
         case Empty: break;  
         case Pawn:
            state.moveCount = 0;
            switch (moveType)
            {
            case EnPassant:
               // update hash code
               Xor(state.hashCode, start, WhitePawn);
               Xor(state.hashCode, dest, WhitePawn);
               Xor(pawnHashCodeW, start, WhitePawn);
               Xor(pawnHashCodeW, dest, WhitePawn);
                   ASSERT(dest - 8 == old_epsq);
               target = old_epsq;
                           capture = BlackPawn;
               contents[dest] = WhitePawn;
               pawn_bits[White].set(dest);
               break;
            case Promotion:
               // update hash code
               Xor(state.hashCode, start, WhitePawn);
               Xor(state.hashCode, dest, MakeWhitePiece(PromoteTo(move)));
               Xor(pawnHashCodeW, start, WhitePawn);
               contents[dest] = MakeWhitePiece(PromoteTo(move));
               material[White].removePawn();
               material[White].addPiece(PromoteTo(move));
               switch (PromoteTo(move))
               {
               case Knight:
                  knight_bits[White].set(dest);
                  break;
               case Bishop:
                  bishop_bits[White].set(dest);
                  break;
               case Rook:
                  rook_bits[White].set(dest);
                  break;
               case Queen:
                  queen_bits[White].set(dest);
                  break;
               default:
                  break;
               }
               break;
            default:
               Xor(state.hashCode, start, WhitePawn );
               Xor(state.hashCode, dest, WhitePawn );
               Xor(pawnHashCodeW, start, WhitePawn);
               Xor(pawnHashCodeW, dest, WhitePawn);
               contents[dest] = WhitePawn;
               if (dest - start == 16) // 2-square pawn advance
               {
                  if (TEST_MASK(Attacks::ep_mask[File(dest)-1][(int)White],pawn_bits[Black])) {
                    state.enPassantSq = dest;
                    state.hashCode ^= ep_codes[0];
                    state.hashCode ^= ep_codes[dest];
                  }
               }
               pawn_bits[White].set(dest);
               break;
            }
            pawn_bits[White].clear(start);
            break;
         case Knight:
            Xor(state.hashCode, start, WhiteKnight);
            Xor(state.hashCode, dest, WhiteKnight);
            contents[dest] = WhiteKnight;
            knight_bits[White].setClear(bits);
            break;
         case Bishop:
            Xor(state.hashCode, start, WhiteBishop);
            Xor(state.hashCode, dest, WhiteBishop);
            contents[dest] = WhiteBishop;
            bishop_bits[White].setClear(bits);
            break;
         case Rook:
            Xor(state.hashCode, start, WhiteRook );
            Xor(state.hashCode, dest, WhiteRook );
            contents[dest] = WhiteRook;
            rook_bits[White].setClear(bits);
            if ((int)state.castleStatus[White]<3) {
               state.hashCode ^= w_castle_status[(int)state.castleStatus[White]];
               state.castleStatus[White] = UpdateCastleStatusW(state.castleStatus[White],start);
               state.hashCode ^= w_castle_status[(int)state.castleStatus[White]];
            }
            break;
         case Queen:
            Xor(state.hashCode, start, WhiteQueen);
            Xor(state.hashCode, dest, WhiteQueen);
            contents[dest] = WhiteQueen;
            queen_bits[White].setClear(bits);
            break;
         case King:
            Xor(state.hashCode, start, WhiteKing );
            Xor(state.hashCode, dest, WhiteKing );
            contents[dest] = WhiteKing;
            kingPos[White] = dest;
            if ((castleStatus(White) != CastledQSide) &&
                (castleStatus(White) != CastledKSide))
            {
               state.hashCode ^= w_castle_status[(int)castleStatus(White)];
               state.hashCode ^= w_castle_status[(int)CantCastleEitherSide];
               state.castleStatus[White] = CantCastleEitherSide;
            }
            break;
         }
         contents[start] = EmptyPiece;
         if (capture != EmptyPiece) 
         {
            state.moveCount = 0;
            ASSERT(target != InvalidSquare);
            occupied[Black].clear(target);
            Xor(state.hashCode, target, capture);
            switch (TypeOfPiece(capture))
            {
            case Empty: break;  
            case Pawn:
                   ASSERT(pawn_bits[Black].isSet(target));
               pawn_bits[Black].clear(target);
               Xor(pawnHashCodeB, target, capture);
               if (moveType == EnPassant)
               {
                  contents[target] = EmptyPiece;
                  clearAll(Black,target);
               }
               material[Black].removePawn();
               break;
            case Rook:
               rook_bits[Black].clear(target);
               material[Black].removePiece(Rook);
               if ((int)state.castleStatus[Black]<3) {
                  state.hashCode ^= b_castle_status[(int)state.castleStatus[Black]];
                  state.castleStatus[Black] = UpdateCastleStatusB(state.castleStatus[Black],dest);
                  state.hashCode ^= b_castle_status[(int)state.castleStatus[Black]];
               }
               break;
            case Knight:
               knight_bits[Black].clear(target);
               material[Black].removePiece(Knight);
               break;
            case Bishop:
               bishop_bits[Black].clear(target);
               material[Black].removePiece(Bishop);
               break;
            case Queen:
               queen_bits[Black].clear(target);
               material[Black].removePiece(Queen);
               break;
            case King:
               ASSERT(0);
               kingPos[Black] = InvalidSquare;
               state.castleStatus[Black] = CantCastleEitherSide;
               material[Black].removePiece(King);
               break;
            default: 
               break;
            }
         }
      }
      setAll(White,dest);
      clearAll(White,start);

   }
   else // side == Black
   {
      if (moveType == KCastle)
      {
         state.moveCount = 0;
         const Square kp = kingSquare(Black);

         // update the hash code
         Xor(state.hashCode, kp+3, BlackRook);
         Xor(state.hashCode, kp, BlackKing);
         Xor(state.hashCode, kp+1, BlackRook);
         Xor(state.hashCode, kp+2, BlackKing);
         state.hashCode ^= b_castle_status[(int)state.castleStatus[Black]];
         state.hashCode ^= b_castle_status[(int)CastledKSide];

         const int newkp = kp + 2;
         kingPos[Black] = newkp;
         state.castleStatus[Black] = CastledKSide;
         // find old square of rook
         Square oldrooksq = kp + 3;
         Square newrooksq = kp + 1;
         contents[kp] = contents[oldrooksq] = EmptyPiece;
         contents[newrooksq] = BlackRook;
         contents[kp+2] = BlackKing;
         rook_bits[Black].clear(oldrooksq);
         rook_bits[Black].set(newrooksq);
         clearAll(Black,kp);
         clearAll(Black,oldrooksq);
         setAll(Black,newkp);
         setAll(Black,newrooksq);
      }
      else if (moveType == QCastle)
      {
         state.moveCount = 0;
         const Square kp = kingSquare(Black);

         // update the hash code
         Xor(state.hashCode, kp-4, BlackRook);
         Xor(state.hashCode, kp, BlackKing);
         Xor(state.hashCode, kp-1, BlackRook);
         Xor(state.hashCode, kp-2, BlackKing);
         state.hashCode ^= b_castle_status[(int)state.castleStatus[Black]];
         state.hashCode ^= b_castle_status[(int)CastledQSide];

         const int newkp = kp - 2;
         kingPos[Black] = newkp;
         state.castleStatus[Black] = CastledQSide;
         // find old square of rook
         Square oldrooksq = kp - 4;
         Square newrooksq = kp - 1;
         contents[kp] = contents[oldrooksq] = EmptyPiece;
         contents[newrooksq] = BlackRook;
         contents[kp-2] = BlackKing;
         rook_bits[Black].clear(oldrooksq);
         rook_bits[Black].set(newrooksq);
         clearAll(Black,kp);
         clearAll(Black,oldrooksq);
         setAll(Black,newkp);
         setAll(Black,newrooksq);
      }
      else // not castling
      {
         ASSERT(contents[start] != EmptyPiece);
         const Bitboard bits(Bitboard::mask[start] |
                           Bitboard::mask[dest]);
         Square target = dest; // where we captured
                 Piece capture = contents[dest]; // what we captured
         switch (TypeOfPiece(contents[StartSquare(move)])) {
         case Empty: break;  
         case Pawn:
            state.moveCount = 0;
            switch (moveType)
            {
            case EnPassant:
               // update hash code
               Xor(state.hashCode, start, BlackPawn);
               Xor(state.hashCode, dest, BlackPawn);
               Xor(pawnHashCodeB, start, BlackPawn);
               Xor(pawnHashCodeB, dest, BlackPawn);
                   ASSERT(dest + 8 == old_epsq);
               target = old_epsq;
                           capture = WhitePawn;
               contents[dest] = BlackPawn;
               pawn_bits[Black].set(dest);
               break;
            case Promotion:
               // update hash code
               Xor(state.hashCode, start, BlackPawn);
               Xor(state.hashCode, dest, MakeBlackPiece(PromoteTo(move)));
               Xor(pawnHashCodeB, start, BlackPawn);
               contents[dest] = MakeBlackPiece(PromoteTo(move));
               material[Black].removePawn();
               material[Black].addPiece(PromoteTo(move));
               switch (PromoteTo(move))
               {
               case Knight:
                  knight_bits[Black].set(dest);
                  break;
               case Bishop:
                  bishop_bits[Black].set(dest);
                  break;
               case Rook:
                  rook_bits[Black].set(dest);
                  break;
               case Queen:
                  queen_bits[Black].set(dest);
                  break;
               default:
                  break;
               }
               break;
            default:
               Xor(state.hashCode, start, BlackPawn );
               Xor(state.hashCode, dest, BlackPawn );
               Xor(pawnHashCodeB, start, BlackPawn);
               Xor(pawnHashCodeB, dest, BlackPawn);
               contents[dest] = BlackPawn;
               if (start - dest == 16) // 2-square pawn advance
               { 
                  if (TEST_MASK(Attacks::ep_mask[File(dest)-1][(int)Black],pawn_bits[White])) {
                    state.enPassantSq = dest;
                    state.hashCode ^= ep_codes[0];
                    state.hashCode ^= ep_codes[dest];
                  }
               }
               pawn_bits[Black].set(dest);
               break;
            }
            pawn_bits[Black].clear(start);
            break;
         case Knight:
            Xor(state.hashCode, start, BlackKnight);
            Xor(state.hashCode, dest, BlackKnight);
            contents[dest] = BlackKnight;
            knight_bits[Black].setClear(bits);
            break;
         case Bishop:
            Xor(state.hashCode, start, BlackBishop);
            Xor(state.hashCode, dest, BlackBishop);
            contents[dest] = BlackBishop;
            bishop_bits[Black].setClear(bits);
            break;
         case Rook:
            Xor(state.hashCode, start, BlackRook );
            Xor(state.hashCode, dest, BlackRook );
            contents[dest] = BlackRook;
            rook_bits[Black].setClear(bits);
            if ((int)state.castleStatus[Black]<3) {
                state.hashCode ^= b_castle_status[(int)state.castleStatus[Black]];
                state.castleStatus[Black] = UpdateCastleStatusB(state.castleStatus[Black],start);
                state.hashCode ^= b_castle_status[(int)state.castleStatus[Black]];
            }
            break;
         case Queen:
            Xor(state.hashCode, start, BlackQueen);
            Xor(state.hashCode, dest, BlackQueen);
            contents[dest] = BlackQueen;
            queen_bits[Black].setClear(bits);
            break;
         case King:
            Xor(state.hashCode, start, BlackKing );
            Xor(state.hashCode, dest, BlackKing );
            contents[dest] = BlackKing;
            kingPos[Black] = dest;
            if ((castleStatus(Black) != CastledQSide) &&
                (castleStatus(Black) != CastledKSide))
            {
               state.hashCode ^= b_castle_status[(int)castleStatus(Black)];
               state.hashCode ^= b_castle_status[(int)CantCastleEitherSide];
               state.castleStatus[Black] = CantCastleEitherSide;
            }
            break;
         }
         contents[start] = EmptyPiece;
         if (capture != EmptyPiece)
         {
            state.moveCount = 0;
            ASSERT(target != InvalidSquare);
            occupied[White].clear(target);
            Xor(state.hashCode, target, capture);
            switch (TypeOfPiece(capture)) {
            case Empty: break;  
            case Pawn:
                   ASSERT(pawn_bits[White].isSet(target));
               pawn_bits[White].clear(target);
               Xor(pawnHashCodeW, target, capture);
               if (moveType == EnPassant)
               {
                  contents[target] = EmptyPiece;
                  clearAll(White,target);
               }
               material[White].removePawn();
               break;
            case Rook:
               rook_bits[White].clear(target);
               material[White].removePiece(Rook);
               if ((int)state.castleStatus[White]<3) {
                  state.hashCode ^= w_castle_status[(int)state.castleStatus[White]];
                  state.castleStatus[White] = UpdateCastleStatusW(state.castleStatus[White],dest);
                  state.hashCode ^= w_castle_status[(int)state.castleStatus[White]];
               }
               break;
            case Knight:
               knight_bits[White].clear(target);
               material[White].removePiece(Knight);
               break;
            case Bishop:
               bishop_bits[White].clear(target);
               material[White].removePiece(Bishop);
               break;
            case Queen:
               queen_bits[White].clear(target);
               material[White].removePiece(Queen);
               break;
            case King:
               ASSERT(0);
               kingPos[White] = InvalidSquare;
               state.castleStatus[White] = CantCastleEitherSide;
               material[White].removePiece(King);
               break;
            default: 
               break;
            }
         }
         setAll(Black,dest);
         clearAll(Black,start);
      }
   }

   if (sideToMove() == White)
      state.hashCode |= (hash_t)1;
   else
      state.hashCode &= (hash_t)~1;
   *repListHead++ = state.hashCode;
   //ASSERT(pawn_hash(White) == BoardHash::pawnHash(*this),White);
   ASSERT(getMaterial(sideToMove()).pawnCount() == (int)pawn_bits[side].bitCount());
   side = oppositeSide();
   ASSERT(getMaterial(sideToMove()).pawnCount() == (int)pawn_bits[side].bitCount());
   allOccupied = occupied[White] | occupied[Black];
   ASSERT(state.hashCode == BoardHash::hashCode(*this));
#if defined(_DEBUG) && defined(FULL_DEBUG)
   // verify correct updating of bitmaps:
   Board copy(*this);
   copy.setSecondaryVars();
   ASSERT(pawn_bits[White] == copy.pawn_bits[White]);
   ASSERT(knight_bits[White] == copy.knight_bits[White]);
   ASSERT(bishop_bits[White] == copy.bishop_bits[White]);
   ASSERT(rook_bits[White] == copy.rook_bits[White]);
   ASSERT(queen_bits[White] == copy.queen_bits[White]);
   ASSERT(occupied[White] == copy.occupied[White]);

   ASSERT(pawn_bits[Black] == copy.pawn_bits[Black]);
   ASSERT(knight_bits[Black] == copy.knight_bits[Black]);
   ASSERT(bishop_bits[Black] == copy.bishop_bits[Black]);
   ASSERT(rook_bits[Black] == copy.rook_bits[Black]);
   ASSERT(queen_bits[Black] == copy.queen_bits[Black]);
   ASSERT(occupied[Black] == copy.occupied[Black]);
   ASSERT(contents[kingPos[White]]==WhiteKing);
   ASSERT(contents[kingPos[Black]]==BlackKing);
#endif     
}

hash_t Board::hashCode( Move move ) const
{
   hash_t newHash = state.hashCode;
   if (state.enPassantSq != InvalidSquare)
   {
       newHash ^= ep_codes[state.enPassantSq];
       newHash ^= ep_codes[0];
   }
   const Square start = StartSquare(move);
   const Square dest = DestSquare(move);
   const MoveType moveType = TypeOfMove(move);
   if (side == White)
   {
      if (moveType == KCastle)
      {
         const Square kp = kingSquare(White);
         Xor(newHash, kp+3, WhiteRook);
         Xor(newHash, kp, WhiteKing);
         Xor(newHash, kp+1, WhiteRook);
         Xor(newHash, kp+2, WhiteKing);
         newHash ^= w_castle_status[(int)state.castleStatus[White]];
         newHash ^= w_castle_status[(int)CastledKSide];
      }
      else if (moveType == QCastle)
      {
         const Square kp = kingSquare(White);
         Xor(newHash, kp-4, WhiteRook);
         Xor(newHash, kp, WhiteKing);
         Xor(newHash, kp-1, WhiteRook);
         Xor(newHash, kp-2, WhiteKing);
         newHash ^= w_castle_status[(int)state.castleStatus[White]];
         newHash ^= w_castle_status[(int)CastledQSide];
      }
      else // not castling
      {
         Square target = dest; // where we captured
         switch (TypeOfPiece(contents[StartSquare(move)]))
         {
         case Empty: break;  
         case Pawn:
            switch (moveType)
            {
            case EnPassant:
               // update hash code
               Xor(newHash, start, WhitePawn);
               Xor(newHash, dest, WhitePawn);
               target = state.enPassantSq;
               break;
            case Promotion:
               // update hash code
               Xor(newHash, start, WhitePawn);
               Xor(newHash, dest, MakeWhitePiece(PromoteTo(move)));
               break;
            default:
               Xor(newHash, start, WhitePawn );
               Xor(newHash, dest, WhitePawn );
               if (start - dest == 16) // 2-square pawn advance
               {
                  if (TEST_MASK(Attacks::ep_mask[File(dest)-1][(int)White],pawn_bits[Black])) {
                    newHash ^= ep_codes[0];
                    newHash ^= ep_codes[dest];
                  }
               }
               break;
            }
            break;
         case Knight:
            Xor(newHash, start, WhiteKnight);
            Xor(newHash, dest, WhiteKnight);
            break;
         case Bishop:
            Xor(newHash, start, WhiteBishop);
            Xor(newHash, dest, WhiteBishop);
            break;
         case Rook:
            Xor(newHash, start, WhiteRook );
            Xor(newHash, dest, WhiteRook );
            if ((int)state.castleStatus[White]<3) {
               newHash ^= w_castle_status[(int)state.castleStatus[White]];
               newHash ^= w_castle_status[(int)UpdateCastleStatusW(state.castleStatus[White],start)];
            }
            break;
         case Queen:
            Xor(newHash, start, WhiteQueen);
            Xor(newHash, dest, WhiteQueen);
            break;
         case King:
            Xor(newHash, start, WhiteKing );
            Xor(newHash, dest, WhiteKing );
            if ((castleStatus(White) != CastledQSide) &&
                (castleStatus(White) != CastledKSide))
            {
               newHash ^= w_castle_status[(int)castleStatus(White)];
               newHash ^= w_castle_status[(int)CantCastleEitherSide];
            }
            break;
         }
         if (Capture(move) != Empty) 
         {
            Piece cap = MakeBlackPiece(Capture(move));
            Xor(newHash, target, cap);
            if (Capture(move) == Rook) {
               if ((int)state.castleStatus[Black]<3) {
                  newHash ^= b_castle_status[(int)state.castleStatus[Black]];
                  newHash ^= b_castle_status[(int)UpdateCastleStatusB(state.castleStatus[Black],dest)];
               }
            }
         }
      }

   }
   else // side == Black
   {
      if (moveType == KCastle)
      {
         const Square kp = kingSquare(Black);
         Xor(newHash, kp+3, BlackRook);
         Xor(newHash, kp, BlackKing);
         Xor(newHash, kp+1, BlackRook);
         Xor(newHash, kp+2, BlackKing);
         newHash ^= b_castle_status[(int)state.castleStatus[Black]];
         newHash ^= b_castle_status[(int)CastledKSide];
      }
      else if (moveType == QCastle)
      {
         const Square kp = kingSquare(Black);
         Xor(newHash, kp-4, BlackRook);
         Xor(newHash, kp, BlackKing);
         Xor(newHash, kp-1, BlackRook);
         Xor(newHash, kp-2, BlackKing);
         newHash ^= b_castle_status[(int)state.castleStatus[Black]];
         newHash ^= b_castle_status[(int)CastledQSide];
      }
      else // not castling
      {
         Square target = dest; // where we captured
         switch (TypeOfPiece(contents[StartSquare(move)]))
         {
         case Empty: break;  
         case Pawn:
            switch (moveType)
            {
            case EnPassant:
               // update hash code
               Xor(newHash, start, BlackPawn);
               Xor(newHash, dest, BlackPawn);
               target = state.enPassantSq;
               break;
            case Promotion:
               // update hash code
               Xor(newHash, start, BlackPawn);
               Xor(newHash, dest, MakeBlackPiece(PromoteTo(move)));
               break;
            default:
               Xor(newHash, start, BlackPawn );
               Xor(newHash, dest, BlackPawn );
               if (dest - start == 16) // 2-square pawn advance
               { 
                  if (TEST_MASK(Attacks::ep_mask[File(dest)-1][(int)Black],pawn_bits[White])) {
                    newHash ^= ep_codes[0];
                    newHash ^= ep_codes[dest];
                  }
               }
               break;
            }
            break;
         case Knight:
            Xor(newHash, start, BlackKnight);
            Xor(newHash, dest, BlackKnight);
            break;
         case Bishop:
            Xor(newHash, start, BlackBishop);
            Xor(newHash, dest, BlackBishop);
            break;
         case Rook:
            Xor(newHash, start, BlackRook );
            Xor(newHash, dest, BlackRook );
            if ((int)state.castleStatus[Black]<3) {
               newHash ^= b_castle_status[(int)state.castleStatus[Black]];
               newHash ^= b_castle_status[(int)UpdateCastleStatusB(state.castleStatus[Black],dest)];
            }
            break;
         case Queen:
            Xor(newHash, start, BlackQueen);
            Xor(newHash, dest, BlackQueen);
            break;
         case King:
            Xor(newHash, start, BlackKing );
            Xor(newHash, dest, BlackKing );
            if ((castleStatus(Black) != CastledQSide) &&
                (castleStatus(Black) != CastledKSide))
            {
               newHash ^= b_castle_status[(int)castleStatus(Black)];
               newHash ^= b_castle_status[(int)CantCastleEitherSide];
            }
            break;
         }
         if (Capture(move) != Empty)
         {
            Piece cap = MakeWhitePiece(Capture(move));
            Xor(newHash, target, cap);
            if (Capture(move) == Rook) {
               if ((int)state.castleStatus[White]<3) {
                  newHash ^= w_castle_status[(int)state.castleStatus[White]];
                  newHash ^= w_castle_status[(int)UpdateCastleStatusW(state.castleStatus[White],dest)];
               }
            }
         }

      }
   }

   if (sideToMove() == White)
      newHash |= (hash_t)1;
   else
      newHash &= (hash_t)~1;
   return newHash;
}

void Board::undoCastling(Square kp, Square oldkingsq, Square newrooksq,
                          Square oldrooksq)
{
   contents[kp] = EmptyPiece;
   contents[oldrooksq] = MakePiece(Rook,side);
   contents[newrooksq] = EmptyPiece;
   contents[oldkingsq] = MakePiece(King,side);
   kingPos[side] = oldkingsq;
   rook_bits[side].set(oldrooksq);
   rook_bits[side].clear(newrooksq);

   setAll(side,oldrooksq);
   setAll(side,oldkingsq);
   clearAll(side,kp);
   clearAll(side,newrooksq);
}

void Board::undoMove( Move move, const BoardState &old_state )
{
   side = OppositeColor(side);
   if (!IsNull(move))
   {
      const MoveType moveType = TypeOfMove(move);
      const Square start = StartSquare(move);
      const Square dest = DestSquare(move);
      if (moveType == KCastle)
      {
         Square kp = kingSquare(side);
         Square oldrooksq = kp+1;
         Square newrooksq = kp-1;
         Square oldkingsq = kp-2;
         undoCastling(kp,oldkingsq,newrooksq,oldrooksq);
      }
      else if (moveType == QCastle)
      {
         Square kp = kingSquare(side);
         Square oldrooksq = kp-2;
         Square newrooksq = kp+1;
         Square oldkingsq = kp+2;
         undoCastling(kp,oldkingsq,newrooksq,oldrooksq);
      }
      else if (side == White)
      {
         const Bitboard bits(Bitboard::mask[start] |
                           Bitboard::mask[dest]);
         // not castling
         Square target = dest;
         // fix up start square:
         if (moveType == Promotion || moveType == EnPassant)
         {
            contents[start] = WhitePawn;
         }
         else
         {
            contents[start] = contents[dest];
         }
         setAll(White,start);
         switch (TypeOfPiece(contents[start])) {
         case Empty: break;  
         case Pawn:
            Xor(pawnHashCodeW,start,WhitePawn);
            switch (moveType) {
            case Promotion:
               material[White].addPawn();
               material[White].removePiece(PromoteTo(move));
               switch (PromoteTo(move))
               {
               case Knight:
                  knight_bits[White].clear(dest);
                  break;
               case Bishop:
                  bishop_bits[White].clear(dest);
                  break;
               case Rook:
                  rook_bits[White].clear(dest);
                  break;
               case Queen:
                  queen_bits[White].clear(dest);
                  break;
               default:
                  break;
               }
               break;
            case EnPassant:
              target = dest - 8; 
                  ASSERT(OnBoard(target));
                  ASSERT(contents[target]==EmptyPiece);
            case Normal:
              pawn_bits[White].clear(dest);
              Xor(pawnHashCodeW,dest,WhitePawn);
            default: break;
            }
            pawn_bits[White].set(start);
            break;
         case Knight:
            knight_bits[White].setClear(bits);
            break;
         case Bishop:
            bishop_bits[White].setClear(bits);
            break;
         case Rook:
            rook_bits[White].setClear(bits);
            break;
         case Queen:
            queen_bits[White].setClear(bits);
            break;
         case King:
            kingPos[White] = start;
            break;
         default:
            break;
         }
         // fix up dest square
         clearAll(White,dest);
         contents[dest] = EmptyPiece;
         contents[target] = MakePiece(Capture(move),Black);
         if (Capture(move) != Empty)
         {
            switch (Capture(move))
            {
            case Pawn:
                           ASSERT(!pawn_bits[Black].isSet(target));
               pawn_bits[Black].set(target);
               Xor(pawnHashCodeB,target,BlackPawn);
               material[Black].addPawn();
               break;
            case Knight:
               knight_bits[Black].set(target);
               material[Black].addPiece(Knight);
               break;
            case Bishop:
               bishop_bits[Black].set(target);
               material[Black].addPiece(Bishop);
               break;
            case Rook:
               rook_bits[Black].set(target);
               material[Black].addPiece(Rook);
               break;
            case Queen:
               queen_bits[Black].set(target);
               material[Black].addPiece(Queen);
               break;
            case King:
               kingPos[Black] = target;
               material[Black].addPiece(King);
               break;
            default:
               break;
            }
            setAll(Black,target);
         }
      }
      else // side == Black
      {
         const Bitboard bits(Bitboard::mask[start] |
                           Bitboard::mask[dest]);
         // not castling
         Square target = dest;
         // fix up start square:
         if (moveType == Promotion || moveType == EnPassant)
         {
            contents[start] = BlackPawn;
         }
         else
         {
            contents[start] = contents[dest];
         }
         setAll(Black,start);
         switch (TypeOfPiece(contents[start])) {
         case Empty: break;  
         case Pawn:
            Xor(pawnHashCodeB,start,BlackPawn);
            switch (moveType) {
            case Promotion:
            {
               material[Black].addPawn();
               material[Black].removePiece(PromoteTo(move));
               switch (PromoteTo(move))
               {
               case Knight:
                  knight_bits[Black].clear(dest);
                  break;
               case Bishop:
                  bishop_bits[Black].clear(dest);
                  break;
               case Rook:
                  rook_bits[Black].clear(dest);
                  break;
               case Queen:
                  queen_bits[Black].clear(dest);
                  break;
               default:
                  break;
               }
                           break;
                        }
            case EnPassant:
              target = dest + 8;
                          ASSERT(OnBoard(target));
                          ASSERT(contents[target]== EmptyPiece);
            case Normal:
              pawn_bits[Black].clear(dest);
              Xor(pawnHashCodeB,dest,BlackPawn);
            default: break;
            }
            pawn_bits[Black].set(start);
            break;
         case Knight:
            knight_bits[Black].setClear(bits);
            break;
         case Bishop:
            bishop_bits[Black].setClear(bits);
            break;
         case Rook:
            rook_bits[Black].setClear(bits);
            break;
         case Queen:
            queen_bits[Black].setClear(bits);
            break;
         case King:
            kingPos[Black] = start;
            break;
         default:
            break;
         }
         // fix up dest square
         clearAll(Black,dest);
         contents[dest] = EmptyPiece;
         contents[target] = MakePiece(Capture(move),White);
         if (Capture(move) != Empty)
         {
            switch (Capture(move))
            {
            case Pawn:
                           ASSERT(!pawn_bits[White].isSet(target));
               pawn_bits[White].set(target);
               Xor(pawnHashCodeW,target,WhitePawn);
               material[White].addPawn();
               break;
            case Knight:
               knight_bits[White].set(target);
               material[White].addPiece(Knight);
               break;
            case Bishop:
               bishop_bits[White].set(target);
               material[White].addPiece(Bishop);
               break;
            case Rook:
               rook_bits[White].set(target);
               material[White].addPiece(Rook);
               break;
            case Queen:
               queen_bits[White].set(target);
               material[White].addPiece(Queen);
               break;
            case King:
               kingPos[White] = target;
               material[White].addPiece(King);
               break;
            default:
               break;
            }
            setAll(White,target);
         }
      }
   }
   state = old_state;
   ASSERT(getMaterial(sideToMove()).pawnCount() == (int)pawn_bits[side].bitCount());
   ASSERT(getMaterial(oppositeSide()).pawnCount() == (int)pawn_bits[oppositeSide()].bitCount());
   
   --repListHead;
   allOccupied = Bitboard(occupied[White] | occupied[Black]);
   ASSERT(state.hashCode == BoardHash::hashCode(*this));
#if defined(_DEBUG) && defined(FULL_DEBUG)
   // verify correct updating of bitmaps:
   Board copy = *this;
   ASSERT(pawn_bits[White] == copy.pawn_bits[White]);
   ASSERT(knight_bits[White] == copy.knight_bits[White]);
   ASSERT(bishop_bits[White] == copy.bishop_bits[White]);
   ASSERT(rook_bits[White] == copy.rook_bits[White]);
   ASSERT(queen_bits[White] == copy.queen_bits[White]);
   ASSERT(occupied[White] == copy.occupied[White]);

   ASSERT(pawn_bits[Black] == copy.pawn_bits[Black]);
   ASSERT(knight_bits[Black] == copy.knight_bits[Black]);
   ASSERT(bishop_bits[Black] == copy.bishop_bits[Black]);
   ASSERT(rook_bits[Black] == copy.rook_bits[Black]);
   ASSERT(queen_bits[Black] == copy.queen_bits[Black]);
   ASSERT(occupied[Black] == copy.occupied[Black]);
   ASSERT(contents[kingPos[White]]==WhiteKing);
   ASSERT(contents[kingPos[Black]]==BlackKing);
#endif     
}

int Board::wouldAttack(Move m,Square target) const {
  Bitboard attacks;
  Square sq = DestSquare(m);
  switch(PieceMoved(m)) {
  case Empty: break;
  case Pawn:
    attacks = Attacks::pawn_attacks[sq][side];
    break;
  case Knight:
    attacks = Attacks::knight_attacks[sq];
    break;
  case Bishop:
    attacks = bishopAttacks(sq); break;
  case Rook:
    attacks = rookAttacks(sq); break;
  case Queen:
    attacks = bishopAttacks(sq) | rookAttacks(sq); break;
  case King:
    attacks = Attacks::king_attacks[sq];
    break;
  }
  return attacks.isSet(target);
}

int Board::anyAttacks(const Square sq, ColorType side) const
{
   if (sq == InvalidSquare)
      return 0;
   if (TEST_MASK(Attacks::pawn_attacks[sq][side],pawn_bits[side])) return 1;
   if (TEST_MASK(Attacks::knight_attacks[sq],knight_bits[side])) return 1;
   if (Attacks::king_attacks[sq].isSet(kingSquare(side))) return 1;
   if (TEST_MASK(rook_bits[side] | queen_bits[side],rookAttacks(sq))) return 1;
   if (TEST_MASK(bishop_bits[side] | queen_bits[side],bishopAttacks(sq))) return 1;
   return 0;
}

int Board::anyAttacks(Square sq, ColorType side, Bitboard &source) const
{
   if (sq == InvalidSquare)
      return 0;
   source = Bitboard(Attacks::pawn_attacks[sq][side] & pawn_bits[side]);
   if (!source.is_clear()) return 1;
   source = Bitboard(Attacks::knight_attacks[sq] & knight_bits[side]);
   if (!source.is_clear()) return 1;
   source = Bitboard((uint64)Attacks::king_attacks[sq] & (1ULL<<kingSquare(side)));
   if (!source.is_clear()) return 1;
   source = Bitboard((rook_bits[side] | queen_bits[side]) & rookAttacks(sq));
   if (!source.is_clear()) return 1;
   source = Bitboard((bishop_bits[side] | queen_bits[side]) & bishopAttacks(sq));
   return !source.is_clear();
}

Bitboard Board::calcAttacks(Square sq, ColorType side) const
{
   Bitboard retval;

   retval |= (Attacks::pawn_attacks[sq][side] & pawn_bits[side]);
   retval |= (Attacks::knight_attacks[sq] & knight_bits[side]);
   retval |= (Attacks::king_attacks[sq] & (1ULL<<kingSquare(side)));
   retval |= (rookAttacks(sq) & (rook_bits[side] | queen_bits[side]));
   retval |= (bishopAttacks(sq) & (bishop_bits[side] | queen_bits[side]));

   return retval;
}

Bitboard Board::calcBlocks(Square sq, ColorType side) const
{
   Bitboard retval;

   if (side == Black)
   {
       Square origin = sq-8;
       if (OnBoard(origin) && contents[origin] == BlackPawn)
          retval.set(origin);
       if (Rank<Black>(sq) == 4 && contents[origin] == EmptyPiece &&
           contents[origin - 8] == BlackPawn)
          retval.set(origin - 8);
   }
   else
   {
       Square origin = sq+8;
       if (OnBoard(origin) && contents[origin] == WhitePawn)
          retval.set(origin);
       if (Rank<White>(sq) == 4 && contents[origin] == EmptyPiece &&
          contents[origin + 8] == WhitePawn)
          retval.set(origin + 8);
   }
   
   retval |= (Attacks::knight_attacks[sq] & knight_bits[side]);
   retval |= (rookAttacks(sq) & (rook_bits[side] | queen_bits[side]));
   retval |= (bishopAttacks(sq) & (bishop_bits[side] | queen_bits[side]));

   return retval;
}


Square Board::minAttacker(Bitboard atcks, ColorType side) const
{
   if (side == White)
   {
      Bitboard retval(atcks & pawn_bits[White]);
      if (!retval.is_clear())
         return retval.firstOne();
      retval = (atcks & knight_bits[White]);
      if (!retval.is_clear())
         return retval.firstOne();
      retval = (atcks & bishop_bits[White]);
      if (!retval.is_clear())
         return retval.firstOne();
      retval = (atcks & rook_bits[White]);
      if (!retval.is_clear())
         return retval.firstOne();
      retval = (atcks & queen_bits[White]);
      if (!retval.is_clear())
         return retval.firstOne();
      if (atcks.isSet(kingSquare(White)))
        return kingSquare(White);
      else
        return InvalidSquare;
   }
   else
   {
      Bitboard retval(atcks & pawn_bits[Black]);
      if (!retval.is_clear())
         return retval.firstOne();
      retval = (atcks & knight_bits[Black]);
      if (!retval.is_clear())
         return retval.firstOne();
      retval = (atcks & bishop_bits[Black]);
      if (!retval.is_clear())
         return retval.firstOne();
      retval = (atcks & rook_bits[Black]);
      if (!retval.is_clear())
         return retval.firstOne();
      retval = (atcks & queen_bits[Black]);
      if (!retval.is_clear())
         return retval.firstOne();
      if (atcks.isSet(kingSquare(Black)))
        return kingSquare(Black);
      else
        return InvalidSquare;
   }    
}

Bitboard Board::getXRay(Square attack_square, Square square,ColorType side) const {
   int dir = Attacks::directions[attack_square][square];
   if (dir == 0)
      return Bitboard(0);
   switch (dir)
   {
   case -1:
      if (TEST_MASK((rook_bits[side] | queen_bits[side]),Attacks::rank_mask_right[attack_square]))
      {
         return Bitboard(rankAttacksRight(attack_square) & (rook_bits[side] | queen_bits[side]));
      }
      break;
   case 1:
      if (TEST_MASK((rook_bits[side] | queen_bits[side]),Attacks::rank_mask_left[attack_square]))
      {
         return Bitboard(rankAttacksLeft(attack_square) & (rook_bits[side] | queen_bits[side]));
      }
      break;
   case -8:
      if (TEST_MASK((rook_bits[side] | queen_bits[side]),Attacks::file_mask_up[attack_square]))
      {
         return Bitboard(fileAttacksUp(attack_square) & (rook_bits[side] | queen_bits[side]));
      }
      break;
   case 8:
     if (TEST_MASK((rook_bits[side] | queen_bits[side]),Attacks::file_mask_down[attack_square]))
      {
         return Bitboard(fileAttacksDown(attack_square) & (rook_bits[side] | queen_bits[side]));
      }
      break;
   case -7:
     if (TEST_MASK((bishop_bits[side] | queen_bits[side]),Attacks::diag_a8_upper_mask[attack_square]))
      {
         return Bitboard(diagAttacksA8Upper(attack_square) & (bishop_bits[side] | queen_bits[side]));
      }
      break;
   case 7:
     if (TEST_MASK((bishop_bits[side] | queen_bits[side]),Attacks::diag_a8_lower_mask[attack_square]))
      {
        return Bitboard(diagAttacksA8Lower(attack_square) & (bishop_bits[side] | queen_bits[side]));
      }
      break;
   case -9:
     if (TEST_MASK((bishop_bits[side] | queen_bits[side]),Attacks::diag_a1_upper_mask[attack_square]))
      {
         return (diagAttacksA1Upper(attack_square) & (bishop_bits[side] | queen_bits[side]));
      }
      break;
   case 9:
      if (TEST_MASK((bishop_bits[side] | queen_bits[side]),Attacks::diag_a1_lower_mask[attack_square]))
      {
        return Bitboard(diagAttacksA1Lower(attack_square) & (bishop_bits[side] | queen_bits[side]));
      }
      break;
   default:  
      ASSERT(0);
   }
   return Bitboard(0);
}


Bitboard Board::allPawnAttacks( ColorType side) const
{
   if (side == Black)
   {
      Bitboard pawns1(pawn_bits[Black]);
      Bitboard pawns2(pawns1);
      pawns1.shr(7);
      pawns1 &= Bitboard(~0x0101010101010101ULL);
      pawns2.shr(9);
      pawns2 &= Bitboard(~0x8080808080808080ULL);
      return (pawns1 | pawns2);
   }
   else
   {
      Bitboard pawns1(pawn_bits[White]);
      Bitboard pawns2(pawns1);
      pawns1.shl(7);
      pawns1 &= Bitboard(~0x8080808080808080ULL);
      pawns2.shl(9);
      pawns2 &= Bitboard(~0x0101010101010101ULL);
      return (pawns1 | pawns2);
   }
}

const Bitboard Board::rookAttacks(Square sq,ColorType side) const {
   Board &b = (Board&)*this;
   b.allOccupied &= ~(rook_bits[side] | queen_bits[side]);
   Bitboard attacks(rookAttacks(sq));
   b.allOccupied |= (rook_bits[side] | queen_bits[side]);
   return attacks;
}

const Bitboard Board::bishopAttacks(Square sq,ColorType side) const {
   Board &b = (Board&)*this;
   b.allOccupied &= ~queen_bits[side];
   Bitboard attacks(bishopAttacks(sq));
   b.allOccupied |= queen_bits[side];
   return attacks;
}

CheckStatusType Board::getCheckStatus() const
{
   if (anyAttacks(kingSquare(sideToMove()),oppositeSide()))
   {
      // This is a const function, but we cache its result
      Board &b = (Board&)*this;
      b.state.checkStatus = InCheck;
   }
   else
   {
      // This is a const function, but we cache its result
      Board &b = (Board&)*this;
      b.state.checkStatus = NotInCheck;
   }
   return state.checkStatus;
}

// This variant of CheckStatus sees if the last move made
// delivered check. It is generally faster than CheckStatus
// with no param, because we can use the last move information
// to avoid calling anyAttacks, in many cases.
CheckStatusType Board::checkStatus(Move lastMove) const
{
   if (state.checkStatus != CheckUnknown)
   {
      return state.checkStatus;
   }
   if (IsNull(lastMove))
      return checkStatus();
   Square kp = kingPos[side];
   Square checker = DestSquare(lastMove);
   int d = Attacks::directions[checker][kp];
   Board &b = (Board&)*this;
   switch(PieceMoved(lastMove)) {
      case Pawn: {
          if (TypeOfMove(lastMove) != Normal)
             return checkStatus();
          if (Attacks::pawn_attacks[kp][oppositeSide()].isSet(checker)) {
              b.state.checkStatus = InCheck;
          }
          else if (Attacks::directions[kp][StartSquare(lastMove)] == 0) {
              b.state.checkStatus = NotInCheck;
          }
          if (state.checkStatus == CheckUnknown)
              return checkStatus();
          else
              return state.checkStatus;
       }
       case Rook:
        switch(d) {
          case 1:
              if (rankAttacksRight(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case -1:
              if (rankAttacksLeft(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case 8:
              if (fileAttacksUp(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case -8:
              if (fileAttacksDown(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
          default:
            break;
        }
        if (state.checkStatus == CheckUnknown) {
          // see if move could generate a discovered check
            d = Util::Abs(Attacks::directions[kp][StartSquare(lastMove)]);
            switch(d) {
            case 0:
            case 1:
            case 8:
                 b.state.checkStatus = NotInCheck;
                 return NotInCheck;
            case 7: {
              Bitboard attacks = diagAttacksA8(StartSquare(lastMove));
              if (attacks.isSet(kp) && TEST_MASK(attacks,
                                                 (bishop_bits[oppositeSide()] | queen_bits[oppositeSide()])))
                  b.state.checkStatus = InCheck;
              else
                  b.state.checkStatus = NotInCheck;
              
            }
              break;
            case 9: {
              Bitboard attacks = diagAttacksA1(StartSquare(lastMove));
              if (attacks.isSet(kp) && TEST_MASK(attacks,
                                                 (bishop_bits[oppositeSide()] | queen_bits[oppositeSide()])))
                  b.state.checkStatus = InCheck;
              else
                  b.state.checkStatus = NotInCheck;
              
            }
              break;
            default:
              break;
                        } // end switch
            }
        else
            return state.checkStatus;
   case Bishop: 
        switch(d) {
          case 7:
              if (diagAttacksA8Upper(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case -7:
              if (diagAttacksA8Lower(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case 9:
              if (diagAttacksA1Upper(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case -9:
              if (diagAttacksA1Lower(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
          default:
            break;
        }
        if (state.checkStatus == CheckUnknown) {
          // see if move could generate a discovered check
            d = Util::Abs(Attacks::directions[kp][StartSquare(lastMove)]);
            switch(d) {
            case 0:
            case 7:
            case 9: {
                 b.state.checkStatus = NotInCheck;
                 return NotInCheck;
            }
              break;
            case 8:{
              Bitboard attacks(fileAttacks(StartSquare(lastMove)));
              if (attacks.isSet(kp) && TEST_MASK(attacks,
                                                 (rook_bits[oppositeSide()] | queen_bits[oppositeSide()])))
                  b.state.checkStatus = InCheck;
              else
                  b.state.checkStatus = NotInCheck;
              
            }
             break;
            case 1: {
              Bitboard attacks(rankAttacks(StartSquare(lastMove)));
              if (attacks.isSet(kp) && TEST_MASK(attacks,
                                                 (rook_bits[oppositeSide()] | queen_bits[oppositeSide()])))
                  b.state.checkStatus = InCheck;
              else
                  b.state.checkStatus = NotInCheck;
              
            }
              break;
            default:
                return checkStatus();
            } // end switch
        }
        else
            return state.checkStatus;
        break;
      case Knight:
        if (Attacks::knight_attacks[checker].isSet(kp))
        {
           b.state.checkStatus = InCheck;
        }
        else {
            d = Util::Abs(Attacks::directions[kp][StartSquare(lastMove)]);
            if (d == 0)
            {
               b.state.checkStatus = NotInCheck;
            }
            else
               return checkStatus();
        }
        break;
        case Queen: {
          b.state.checkStatus = NotInCheck;     
         switch(d) {
         case 0: 
            break;
          case 1:
              if (rankAttacksRight(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case -1:
              if (rankAttacksLeft(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case 8:
              if (fileAttacksUp(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case -8:
              if (fileAttacksDown(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
          case 7:
              if (diagAttacksA8Upper(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case -7:
              if (diagAttacksA8Lower(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case 9:
              if (diagAttacksA1Upper(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
              break;
          case -9:
              if (diagAttacksA1Lower(checker).isSet(kp)) {
                 b.state.checkStatus = InCheck;
              }
          default:
            break;
          }
        }
        break;
      case King: 
          if (TypeOfMove(lastMove) != Normal) /* castling */
              return checkStatus();
          if (Attacks::king_attacks[checker].isSet(kp)) {
            b.state.checkStatus = InCheck;
            return InCheck;
          }
          else if (Attacks::directions[StartSquare(lastMove)][kp] == 0) {
            b.state.checkStatus = NotInCheck;
            return NotInCheck;
          }
          else
            return checkStatus();
      default:
          break;
   }
   return checkStatus();
}

CheckStatusType Board::wouldCheck(Move lastMove) const {
   Square kp = kingPos[oppositeSide()];
   const Square checker = DestSquare(lastMove);
   const int d = (int)Attacks::directions[checker][kp];
   // check for discovered check first
   if (isPinned(oppositeSide(),lastMove)) return InCheck;
   switch(PieceMoved(lastMove)) {
      case Pawn: {
          switch (TypeOfMove(lastMove)) {
          case EnPassant:
             if (Attacks::pawn_attacks[kp][sideToMove()].isSet(checker)) {
                 return InCheck;
             }
             return CheckUnknown;             
          case Promotion:
             // see if the promoted to piece would check the King:
             switch(PromoteTo(lastMove)) {
             case Knight:
                 return Attacks::knight_attacks[checker].isSet(kp) ?
                 InCheck : NotInCheck;
             case Bishop:
                 if (Util::Abs(d) == 7 || Util::Abs(d) == 9) {
                     Board &b = (Board&)*this;
                     b.allOccupied.clear(StartSquare(lastMove));
                     int in_check = bishopAttacks(checker).isSet(kp);
                     b.allOccupied.set(StartSquare(lastMove));
                     return in_check ? InCheck : NotInCheck;
                 } else {
                     return NotInCheck;
                 }
             case Rook:
                 if (Util::Abs(d) == 1 || Util::Abs(d) == 8) {
                     Board &b = (Board&)*this;
                     b.allOccupied.clear(StartSquare(lastMove));
                     int in_check = rookAttacks(checker).isSet(kp);
                     b.allOccupied.set(StartSquare(lastMove));
                     return in_check ? InCheck : NotInCheck;
                 }
                 else {
                     return NotInCheck;
                 }
             case Queen:
                 if (d) {
                     Board &b = (Board&)*this;
                     b.allOccupied.clear(StartSquare(lastMove));
                     int in_check = queenAttacks(checker).isSet(kp);
                     b.allOccupied.set(StartSquare(lastMove));
                     return in_check ? InCheck : NotInCheck;
                 } else {
                     return NotInCheck;
                 }

             default:
              break;
             }
          case Normal: {
             if (Attacks::pawn_attacks[kp][sideToMove()].isSet(checker)) {
                 return InCheck;
             } else {
                 return NotInCheck;
             }
          }
          case KCastle:
          case QCastle:
          break;
          }
       }
       case Knight:
        if (Attacks::knight_attacks[checker].isSet(kp)) {
           return InCheck;
        } else {
           return NotInCheck;
        }
      case King: {
          if (TypeOfMove(lastMove) != Normal) {
             return CheckUnknown;
          } else if (Attacks::king_attacks[DestSquare(lastMove)].isSet(kp)) {
             return InCheck;
          } else {
             return NotInCheck;
          }
       }
       case Bishop:
          switch(d) {
            case 7:
                if (diagAttacksA8Upper(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case -7:
                if (diagAttacksA8Lower(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case 9:
                if (diagAttacksA1Upper(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case -9:
                if (diagAttacksA1Lower(checker).isSet(kp)) {
                   return InCheck;
                }
            default:
              break;
          }
          break;
       case Rook:
          switch(d) {
            case 1:
                if (rankAttacksRight(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case -1:
                if (rankAttacksLeft(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case 8:
                if (fileAttacksUp(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case -8:
                if (fileAttacksDown(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            default:
              break;
          }
          break;
        case Queen: 
          switch(d) {
            case 7:
                if (diagAttacksA8Upper(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case -7:
                if (diagAttacksA8Lower(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case 9:
                if (diagAttacksA1Upper(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case -9:
                if (diagAttacksA1Lower(checker).isSet(kp)) {
                   return InCheck;
                }
            case 1:
                if (rankAttacksRight(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case -1:
                if (rankAttacksLeft(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case 8:
                if (fileAttacksUp(checker).isSet(kp)) {
                   return InCheck;
                }
                break;
            case -8:
                if (fileAttacksDown(checker).isSet(kp)) {
                   return InCheck;
                }
            default:
              break;
          }
          return NotInCheck;
        default:
            break;
   }
   return NotInCheck;
}
   
int Board::wasLegal(Move lastMove) const {
    if (IsNull(lastMove)) return 1;
    Square kp = kingSquare(oppositeSide());
    switch (TypeOfMove(lastMove)) {
       case QCastle:
       case KCastle:
         return 1; // checked for legality in move generator
       case EnPassant:
         return !anyAttacks(kp,sideToMove());
       default:
         break;
    }
    if (PieceMoved(lastMove)==King) {
       return !anyAttacks(kp,sideToMove());
    }
    else {
       Square start = StartSquare(lastMove);
       int dir = Attacks::directions[start][kp];
       if (dir != Attacks::directions[DestSquare(lastMove)][kp]) {
          switch (dir) {
          case 1:
            return !TEST_MASK((rook_bits[side] | queen_bits[side]),rankAttacksLeft(kp));
          case -1:
            return !TEST_MASK((rook_bits[side] | queen_bits[side]),rankAttacksRight(kp));
          case 8:
            return !TEST_MASK((rook_bits[side] | queen_bits[side]),fileAttacksDown(kp));
          case -8:
            return !TEST_MASK((rook_bits[side] | queen_bits[side]),fileAttacksUp(kp));
          case -7:
            return !TEST_MASK((bishop_bits[side] | queen_bits[side]),diagAttacksA8Upper(kp));
          case 7:
            return !TEST_MASK((bishop_bits[side] | queen_bits[side]),diagAttacksA8Lower(kp));
          case -9:
            return !TEST_MASK((bishop_bits[side] | queen_bits[side]),diagAttacksA1Upper(kp));
          case 9:
            return !TEST_MASK((bishop_bits[side] | queen_bits[side]),diagAttacksA1Lower(kp));
          default:
            return 1;
           }
        }
    }
    return 1;
}

int Board::isPinned(ColorType kingColor, Piece p, Square source, Square dest) const
{
   if (p == EmptyPiece || (TypeOfPiece(p) == King && PieceColor(p) == kingColor))
      return 0;
   const Square ks = kingSquare(kingColor);
   const int dir = Attacks::directions[source][ks];
   // check that source is in a line to the King
   if (dir == 0) return 0;
   const int dir2 =  Attacks::directions[dest][ks];
   // check for movement in direction of possible pin
   if (Util::Abs(dir) == Util::Abs(dir2)) return 0;

   Bitboard attacker;
   const ColorType oside = OppositeColor(kingColor);
   switch (dir)
   {
   case 1:
      // Find attacks on the rank
      {
         attacker = Bitboard(rankAttacksLeft(source) &
                     (rook_bits[oside] | queen_bits[oside]));
      }
      break;
   case -1:
      {
         attacker = Bitboard(rankAttacksRight(source) &
                                   (rook_bits[oside] | queen_bits[oside]));
      }
      break;
   case 8:
      {
        attacker = Bitboard(fileAttacksDown(source) &
                                   (rook_bits[oside] | queen_bits[oside]));
      }
      break;
   case -8:
      {
        attacker = Bitboard(fileAttacksUp(source) &
                                   (rook_bits[oside] | queen_bits[oside]));
      }
      break;
   case 7:
      {
        attacker = Bitboard(diagAttacksA8Lower(source) &
                                   (bishop_bits[oside] | queen_bits[oside]));
      }
      break;
   case -7:
      {
        attacker = Bitboard(diagAttacksA8Upper(source) &
                                   (bishop_bits[oside] | queen_bits[oside]));
      }
      break;
   case 9:
      {
        attacker = Bitboard(diagAttacksA1Lower(source) &
                                   (bishop_bits[oside] | queen_bits[oside]));
      }
      break;
   case -9:
      {
        attacker = Bitboard(diagAttacksA1Upper(source) &
                                   (bishop_bits[oside] | queen_bits[oside]));
      }
      break;
   default:  
      break;
   }
   if (attacker) {
      Square attackSq = attacker.firstOne();
      ASSERT(attackSq != InvalidSquare);
      Bitboard btwn(Attacks::betweenSquares[attackSq][ks]);
      btwn.clear(source);
      // pinned if path is clear to the King
      return !(btwn & allOccupied);
   }
   return 0;
}

static void set_bad( istream &i )
{
   i.clear( ios::badbit | i.rdstate() );
}

int Board::repCount(int target) const
{
    int entries = state.moveCount - 2;
    if (entries <= 0) return 0;
    hash_t to_match = hashCode();
    int count = 0;
    for (hash_t *repList=repListHead-3;
       entries>=0;
       repList-=2,entries-=2)
    {
      if (*repList == to_match)
      {
         count++;
         if (count >= target)
         {
            return count;
         }
      }
   }
   return count;
}

Bitboard Board::getPinned(Square ksq, ColorType side) const {
    // Same algorithm as Stockfish: get potential pinners then
    // determine which are actually pinning.
    Bitboard pinners( ((rook_bits[side] | queen_bits[side]) &
                 Attacks::rank_file_mask[ksq]) |
                ((bishop_bits[side] | queen_bits[side]) &
                 Attacks::diag_mask[ksq]));
    Square pinner;
    Bitboard result;
    while (pinners.iterate(pinner)) {
        Bitboard b;
        between(ksq, pinner, b);
        b &= allOccupied;
        if (b.bitCountOpt() == 1) {
            // Only one piece between "pinner" and King. See if it is
            // the correct color.
            result |= b & occupied[side];
        }
    }
    return result;
}

int Board::materialDraw() const {
    // check for insufficient material per FIDE Rules of Chess
    const Material &mat1 = getMaterial(White);
    const Material &mat2 = getMaterial(Black);
    if (mat1.pawnCount() || mat2.pawnCount()) {
        return 0 ;
    }
    if ((mat1.value() <= KING_VALUE + BISHOP_VALUE) &&
    (mat2.value() <= KING_VALUE + BISHOP_VALUE)) {
        if (mat1.kingOnly() || mat2.kingOnly())
            // K vs K, or K vs KN, or K vs KB
            return 1;
        else if (mat1.infobits() == Material::KN &&
        mat2.infobits() == Material::KN) {
            // KN vs KN
            return 0;
        }
        else {                                    /* KBKB */
            // drawn if same-color bishops
            if (TEST_MASK(bishop_bits[White],black_squares))
                return TEST_MASK(bishop_bits[Black],black_squares);
            else if (TEST_MASK(bishop_bits[White],white_squares))
                return TEST_MASK(bishop_bits[Black],white_squares);
        }
    }
    return 0 /*FALSE*/;
}

void Board::flip() {
   for (int i=0;i<4;i++) {
     for (int j=0;j<8;j++) {
        Piece tmp = contents[i*8+j];
        tmp = MakePiece(TypeOfPiece(tmp),OppositeColor(PieceColor(tmp)));
        Piece tmp2 = contents[(7-i)*8+j];
        tmp2 = MakePiece(TypeOfPiece(tmp2),OppositeColor(PieceColor(tmp2)));
        contents[i*8+j] = tmp2;
        contents[(7-i)*8+j] = tmp;
     }
   }
   CastleType tmp = state.castleStatus[White];
   state.castleStatus[White] = state.castleStatus[Black];
   state.castleStatus[Black] = tmp;
   side = OppositeColor(side);
   setSecondaryVars();
}

void Board::flip2() {
   for (int i=1;i<=4;i++) {
     for (int j=1;j<=8;j++) {
       Square sq = MakeSquare(i,j,White);
       Square sq2 = MakeSquare(9-i,j,White);
       Piece tmp = contents[sq];
       contents[sq] = contents[sq2];
       contents[sq2] = tmp;
     }
   }
   setSecondaryVars();
}

istream & operator >> (istream &i, Board &board)
{
   // read in a board position in Forsythe-Edwards (FEN) notation.
   static char buf[128];

   char *bp = buf;
   int c;
   int fields = 0; int count = 0;
   while (i.good() && fields < 4 && (c = i.get()) != '\n' && 
          c != -1 && 
          ++count < 128)
   {
      *bp++ = c;
      if (isspace(c))
         fields++;
   }
   *bp = '\0';
   if (!i)
      return i;
   if (!BoardIO::readFEN(board, buf))
      set_bad(i);
   return i;
}

ostream & operator << (ostream &o, const Board &board)
{
   BoardIO::writeFEN(board,o,1);
   return o;
}

