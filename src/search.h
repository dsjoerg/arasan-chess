// Copyright 1994-2018 by Jon Dart.  All Rights Reserved.

#ifndef _SEARCH_H
#define _SEARCH_H

#include "board.h"
#include "stats.h"
#include "searchc.h"
#include "scoring.h"
#include "movegen.h"
#include "threadp.h"
#include "options.h"
extern "C" {
#include <memory.h>
#include <time.h>
};
#include <atomic>
#include <functional>
#include <list>
#include <random>
using namespace std;

class MoveGenerator;

struct NodeInfo;

// Per-node info, part of search history stack
struct NodeInfo {
    NodeInfo() : cutoff(0),best(NullMove)
        {
        }
    score_t best_score;
    score_t alpha, beta;
    int cutoff;
    int num_quiets;
    int num_legal;
    int flags;
    Move singularMove;
    Move best;
    Move last_move;
    score_t eval, staticEval;
    Move pv[Constants::MaxPly];
    int pv_length;
    Move quiets[Constants::MaxMoves];
#ifdef MOVE_ORDER_STATS
    int best_count;
#endif
    int ply, depth;
    char pad[110];

    int PV() const {
        return (beta > alpha+1);
    }

    int inBounds(score_t score) const {
        return score > alpha && score < beta;
    }
    int newBest(score_t score) const {
        return score > best_score && score < beta;
    }
};

typedef NodeInfo NodeStack[Constants::MaxPly];

// There are 4 levels of verbosity.  Silent mode does no output to
// the console - it is used by the Windows GUI. Debug level is
// used to output debug info. Whisper is used to "whisper"
// comments on a chess server. "Trace" is used in trace mode
// (-t) of the chess server client.
enum TalkLevel { Silent, Debug, Whisper, Trace };

enum SearchType { FixedDepth, TimeLimit, FixedTime };

class SearchController;

class Search {

    friend class ThreadPool;
    friend class SearchController;

public:

    Search(SearchController *c, ThreadInfo *ti);

    virtual ~Search() = default;

    void init(NodeInfo (&ns)[Constants::MaxPly], ThreadInfo *child_ti);

    score_t search(score_t alpha, score_t beta,
                   int ply, int depth, int flags = 0) {
        PUSH(alpha,beta,flags,ply,depth);
        return POP(search());
    }

    // search based on current board & NodeInfo
    score_t search();

    score_t quiesce(score_t alpha, score_t beta,
                    int ply, int depth) {
        PUSHQ(alpha,beta,ply);
        return POP(quiesce(ply,depth));
    }

    score_t quiesce(int ply,int depth);

    int wasTerminated() const {
        return terminate;
    }

    NodeInfo * getNode() const {
        return node;
    }

    void stop() {
        terminate = 1;
    }

    void clearStopFlag() {
        terminate = 0;
    }

    virtual void clearHashTables();

    // We maintain a local copy of the search options, to reduce
    // the need for each thread to query global memory. This
    // forces a reload of that cache from the global options:
    void setSearchOptions();

    score_t drawScore(const Board &board) const;

#ifdef TUNE
    static const int LEARNING_SEARCH_WINDOW;
    static double func( double x );
#endif

    // main entry point for top-level search; non-main threads enter here
    Move ply0_search();

    score_t ply0_search(RootMoveGenerator &, score_t alpha, score_t beta,
                        int iteration_depth,
                        int depth,
                        const MoveSet &exclude,
                        const MoveSet &include);

    bool mainThread() const {
       return ti->index == 0;
    }

protected:

    enum SearchFlags { IID=1, VERIFY=2, EXACT=4, SINGULAR=8, PROBCUT=16 };

    int calcExtensions(const Board &board,
                       NodeInfo *node,
                       CheckStatusType in_check_after_move,
                       int moveIndex,
                       int improving,
                       Move move);

    void storeHash(hash_t hash, Move hash_move, int depth);

    int updateRootMove(const Board &board,
                       NodeInfo *node, Move move, score_t score, int move_index);

    int updateMove(NodeInfo* myNode, Move move, score_t score, int ply);

    void updatePV(const Board &, Move m, int ply);

    void updatePV(const Board &board,NodeInfo *node,NodeInfo *fromNode,Move move, int ply);

    int checkTime();

    void showStatus(const Board &board, Move best, bool faillow, bool failhigh);

    score_t tbScoreAdjust(const Board &board,
                          score_t score, int tb_hit, score_t tb_score) const;

    score_t futilityMargin(int depth) const;

    int lmpCount(int depth, int improving) const;

    score_t razorMargin(int depth) const;

    score_t seePruningMargin(int depth, bool quiet) const;

    FORCEINLINE void PUSH(score_t alpha, score_t beta, int flags,
                          int ply, int depth) {
        ASSERT(ply<Constants::MaxPly);
        ++node;
        node->alpha = node->best_score = alpha;
        node->beta = beta;
        node->flags = flags;
        node->best = NullMove;
        node->num_quiets = node->num_legal = 0;
        node->ply = ply;
        node->depth = depth;
        node->cutoff = 0;
        node->pv[ply] = node->last_move = NullMove;
        node->pv_length = 0;
    }

    FORCEINLINE void PUSHQ(score_t alpha, score_t beta, int ply) {
        ASSERT(ply<Constants::MaxPly);
        ++node;
        node->flags = 0;
        node->ply = ply;
        node->alpha = node->best_score = alpha;
        node->beta = beta;
        node->best = NullMove;
        node->pv[ply] = NullMove;
        node->pv_length = 0;
    }

    FORCEINLINE score_t POP(score_t value)  {
        --node;
        return value;
    }

    void setVariablesFromController();

    void setContemptFromController();

    void setTalkLevelFromController();

    void updateStats(const Board &, NodeInfo *node,int iteration_depth,
		     score_t score);

    void suboptimal(RootMoveGenerator &mg, Move &m, score_t &val);


    SearchController *controller;
    Board board;
    Statistics stats;
    int iterationDepth;
    SearchContext context;
    int terminate;
    int nodeAccumulator;
    NodeInfo *node; // pointer into NodeStack array (external to class)
    Scoring scoring;
    ThreadInfo *ti; // thread now running this search
    // The following variables are maintained as local copies of
    // state from the controller. Placing them in each thread instance
    // helps avoid global variable contention.
    Options::SearchOptions srcOpts;
    ColorType computerSide;
    score_t contempt;
    int age;
    TalkLevel talkLevel;
};

class SearchController {
    friend class Search;

public:
    SearchController();

    ~SearchController();

    typedef std::function<void(const Statistics &)> PostFunction;
    typedef std::function<int(SearchController *,const Statistics &)> MonitorFunction;

    Move findBestMove(
        const Board &board,
        SearchType srcType,
        int time_limit,
        int xtra_time,
        int ply_limit,
        int background,
        int isUCI,
        Statistics &stat_buf,
        TalkLevel t,
        const MoveSet &exclude,
        const MoveSet &include);

    Move findBestMove(
        const Board &board,
        SearchType srcType,
        int time_limit,
        int xtra_time,
        int ply_limit,
        int background,
        int isUCI,
        Statistics &stats,
        TalkLevel t);

    uint64_t getTimeLimit() const {
        if (typeOfSearch == TimeLimit && time_limit != INFINITE_TIME) {
            // time boost/decrease based on search history:
            int64_t extension = bonus_time;
            if (fail_low_root_extend) {
                // presently failing low, allow up to max extra time
                extension += int64_t(xtra_time);
            }
            else if (fail_high_root || fail_high_root_extend) {
                // extend time for fail high, but less than for
                // failing low
                extension += int64_t(xtra_time)/2;
            }
            extension = std::max<int64_t>(-int64_t(time_target),std::min<int64_t>(int64_t(xtra_time),extension));
            return uint64_t(int64_t(time_target) + extension);
        } else {
            return time_limit;
        }
    }

    uint64_t getMaxTime() const {
        return time_target + xtra_time;
    }

    void terminateNow();

    // Set a "post" function that will be called from the
    // search to output status data (for Winboard; also used
    // in test mode). Returns the previous function instance.
    PostFunction registerPostFunction(PostFunction post) {
        PostFunction tmp = post_function;
        post_function = post;
        return tmp;
    }

    // Set a "monitor" function that will be called during the
    // search. This function returns 1 if the search should
    // terminate. This function returns the previous function
    // instance.
    MonitorFunction registerMonitorFunction(MonitorFunction func) {
        MonitorFunction tmp = monitor_function;
        monitor_function = func;
        return tmp;
    }

    TalkLevel getTalkLevel() const {
        return talkLevel;
    }

    void setTalkLevel(TalkLevel t);

    void clearHashTables();

    void resizeHash(size_t newSize);

    void stopAllThreads();

    void clearStopFlags();

    void updateSearchOptions();

    void setBackground(bool b) {
        background = b;
    }

    bool pondering() const noexcept {
        return is_searching && background;
    }

    bool searching() const noexcept {
        return is_searching;
    }

    void setTimeLimit(uint64_t limit,uint64_t xtra) {
        typeOfSearch = TimeLimit;
        time_limit = time_target = limit;
        xtra_time = xtra;
        // re-calculate bonus time
        applySearchHistoryFactors();
    }

    void setContempt(score_t contempt);

    score_t getContempt() const noexcept {
       return contempt;
    }

    // Note: should not call this while searching
    void setThreadCount(int threads);

    ColorType getComputerSide() const {
        return computerSide;
    }

    void uciSendInfos(const Board &, Move move, int move_index, int depth);

    void stop() {
        stopped = true;
    }

    bool wasStopped() const {
        return stopped;
    }

    void setStop(bool status) {
        stopped = status;
    }

    Hash hashTable;

    score_t drawScore(const Board &board) {
      // if we know the opponent's rating (which will be the case if playing
      // on ICC in xboard mode), or if the user has set a contempt value
      // (in UCI mode), factor that into the draw score - a draw against
      // a high-rated opponent is good; a draw against a lower-rated one is bad.
      if (contempt) {
         if (board.sideToMove() == computerSide)
            return -contempt;
         else
            return contempt;
      }
      return 0;
   }

    uint64_t getElapsedTime() const {
       return elapsed_time;
    }

#ifdef SMP_STATS
	double getCpuPercentage() const {
      if (samples)
         return (100.0*threads)/samples;
      else
         return 0.0;
   }
#endif

   void updateGlobalStats(const Statistics &);

   Statistics * getBestThreadStats(bool trace) const;

   uint64_t totalNodes() const {
      return pool->totalNodes();
   }

   uint64_t totalHits() const {
      return pool->totalHits();
   }

   // Adjust time usage after root fail high or fail low. A temporary
   // time extension is done to allow resolution of the fail high/low.
   // Called from main thread.
   void outOfBoundsTimeAdjust();

   // Calculate the time adjustment after a root search iteration has
   // completed (possibly with one or more fail high/fail lows).
   // Called from main thread.
   void historyBasedTimeAdjust(const Statistics &stats);

   // Apply search history factors to adjust time control
   void applySearchHistoryFactors();

   bool mainThreadCompleted() const noexcept {
       return pool->isCompleted(0);
   }

   const Statistics &getGlobalStats() const noexcept {
       return *stats;
   }

#ifdef NUMA
   void recalcBindings() {
       pool->recalcBindings();
   }
#endif

private:

    // pointer to function, called to output status during
    // a search.
    PostFunction post_function;

    MonitorFunction monitor_function;

    // check console input
    int check_input(const Board &);

    unsigned random(unsigned max) {
       std::uniform_int_distribution<unsigned> dist(0,max);
       return dist(random_engine);
    }

    unsigned nextSearchDepth(unsigned current_depth, unsigned thread_id,
        unsigned max_depth);

    int uci;
    int age;
    TalkLevel talkLevel;
    // time limit is nominal time limit in centiseconds
    // time target is actual time to search in centiseconds
    uint64_t time_limit, time_target;
    // Max amount of time we can add if score is dropping:
    uint64_t xtra_time;
    atomic<int64_t> bonus_time;
    bool fail_high_root_extend, fail_low_root_extend, fail_high_root;
    // Factors to use to adjust time up/down based on search history:
    double searchHistoryBoostFactor, searchHistoryReductionFactor;
    int ply_limit;
    atomic<bool> background;
    atomic<bool> is_searching;
    // flag for UCI. When set the search will terminate at the
    // next time check interval:
    bool stopped;
    SearchType typeOfSearch;
    int time_check_counter;
#ifdef SMP_STATS
    int sample_counter;
#endif
    Statistics *stats;
    ColorType computerSide;
    score_t contempt;
    CLOCK_TYPE startTime;
    CLOCK_TYPE last_time;
    ThreadPool *pool;
    Search *rootSearch;
    int tb_root_probes, tb_root_hits;

    MoveSet include;
    MoveSet exclude;

    struct SearchHistory
    {
        Move pv;
        score_t score;

        SearchHistory() : pv(NullMove), score(Constants::INVALID_SCORE)
        {
        }

        SearchHistory(Move m, score_t value) : pv(m), score(value)
        {
        }
    };

    array<SearchHistory,Constants::MaxPly> rootSearchHistory;

#ifdef SYZYGY_TBS
    int tb_hit, tb_dtz;
    score_t tb_score;
#endif

    Board initialBoard;
    score_t initialValue;
    int waitTime; // for strength feature
    int depth_adjust; // for strength feature
    unsigned select_subopt; // for strength feature
    std::mt19937_64 random_engine;

    uint64_t elapsed_time; // in milliseconds
    std::array <unsigned, Constants::MaxPly> search_counts;
    std::mutex search_count_mtx;

#ifdef SMP_STATS
    uint64_t samples, threads;
#endif

};

#endif
