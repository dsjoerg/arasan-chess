// Copyright 2006-2008, 2011, 2017-2018 by Jon Dart. All Rights Reserved.

#include "searchc.h"
#include "search.h"

SearchContext::SearchContext() {
   history = new ButterflyArray<int>();
   counterMoves = new PieceToArray<Move>();
   counterMoveHistory = new PieceTypeToMatrix<int>();
   fuMoveHistory = new PieceTypeToMatrix<int>();
   clear();
}

SearchContext::~SearchContext()
{
   delete history;
   delete counterMoves;
   delete counterMoveHistory;
   delete fuMoveHistory;
}

void SearchContext::clear() {
    clearKiller();
    for (int side = 0; side < 2; side++)
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                (*history)[side][i][j] = 0;
            }
        }
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 64; j++) {
            (*counterMoves)[i][j] = NullMove;
        }
    }
    // clear counter move history
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 64; j++)
            for (int k = 0; k < 8; k++)
                for (int l = 0; l < 64; l++) {
                    (*counterMoveHistory)[i][j][k][l] = 0;
                    (*fuMoveHistory)[i][j][k][l] = 0;
                }
}

void SearchContext::clearKiller() {
   for (int i = 0; i < Constants::MaxPly; i++) {
      killers1[i] = killers2[i] = NullMove;
   }
}

int SearchContext::scoreForOrdering (Move m, NodeInfo *node, ColorType side) const noexcept
{
    int score = (*history)[side][StartSquare(m)][DestSquare(m)];
    if (node->ply>0 && !IsNull((node-1)->last_move)) {
        Move prevMove = (node-1)->last_move;
        score += (*counterMoveHistory)[PieceMoved(prevMove)][DestSquare(prevMove)][PieceMoved(m)][DestSquare(m)];
    }
    if (node->ply>1 && !IsNull((node-2)->last_move)) {
        Move prevMove = (node-2)->last_move;
        score += (*fuMoveHistory)[PieceMoved(prevMove)][DestSquare(prevMove)][PieceMoved(m)][DestSquare(m)];
    }
    return score;
}

static constexpr int MAX_HISTORY_DEPTH = 17;
static constexpr int MAIN_HISTORY_DIVISOR = 324;
static constexpr int HISTORY_DIVISOR = 768;

int SearchContext::bonus(int depth) const noexcept
{
    const int d = depth/DEPTH_INCREMENT;
    return d <= MAX_HISTORY_DEPTH ? d*d + 2*d : 0;
}

void SearchContext::update(int &val, int bonus, int divisor)
{
    ASSERT(std::abs(bonus) < divisor);
    val += 32*bonus - val*std::abs(bonus)/divisor;
}

void SearchContext::updateStats(const Board &board, NodeInfo *node)
{
    // sanity checks
    Move best = node->best;
    ASSERT(!IsNull(best));
    ASSERT(OnBoard(StartSquare(best)) && OnBoard(DestSquare(best)));
    const int b = bonus(node->depth);
    ASSERT(node->num_quiets<Constants::MaxMoves);
    for (int i=0; i<node->num_quiets; i++) {
        const Move m = node->quiets[i];
        auto updateHist = [&](int &val, int divisor) {
            if (MovesEqual(best,m)) {
                update(val,b,divisor);
            }
            else {
                update(val,-b,divisor);
            }
        };

        updateHist((*history)[board.sideToMove()][StartSquare(m)][DestSquare(m)],MAIN_HISTORY_DIVISOR);
        if (node->ply > 0) {
            Move lastMove = (node-1)->last_move;
            if (!IsNull(lastMove)) {
                updateHist((*counterMoveHistory)[PieceMoved(lastMove)][DestSquare(lastMove)][PieceMoved(m)][DestSquare(m)],HISTORY_DIVISOR);
            }
            if (node->ply > 1) {
                Move lastMove = (node-2)->last_move;
                if (!IsNull(lastMove)) {
                    updateHist((*fuMoveHistory)[PieceMoved(lastMove)][DestSquare(lastMove)][PieceMoved(m)][DestSquare(m)],HISTORY_DIVISOR);
                }
            }
        }
    }
}

int SearchContext::getCmHistory(NodeInfo *node, Move move) const noexcept
{
    if (node->ply == 0 || IsNull((node-1)->last_move)) {
        return 0;
    }
    Move prev((node-1)->last_move);
    return (*counterMoveHistory)[PieceMoved(prev)][DestSquare(prev)][PieceMoved(move)][DestSquare(move)];
}

int SearchContext::getFuHistory(NodeInfo *node, Move move) const noexcept
{
    if (node->ply < 2 || IsNull((node-2)->last_move)) {
        return 0;
    }
    Move prev((node-2)->last_move);
    return (*fuMoveHistory)[PieceMoved(prev)][DestSquare(prev)][PieceMoved(move)][DestSquare(move)];
}
