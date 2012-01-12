#ifndef __OP_HPP__
#define __OP_HPP__

#include "utils.hpp"

struct query_stats_t {

    int queries;
    ticks_t worst_latency;

    bool enable_latency_samples;
    reservoir_sample_t<ticks_t> latency_samples;


    query_stats_t() : queries(0), worst_latency(0), enable_latency_samples(true) { }

    void reset() {
        queries = 0;
        worst_latency = 0;

        latency_samples.clear();
    }

    void push(ticks_t latency, int batch_count) {
        lock.lock();
        queries += batch_count;
        worst_latency = std::max(worst_latency, latency);
        if (enable_latency_samples) {
            for (int i = 0; i < batch_count; i++) latency_samples.push(latency);
        }
        lock.unlock();
    }

    void aggregate(const query_stats_t &other) {
        queries += other.queries;
        worst_latency = std::max(worst_latency, other.worst_latency);
        latency_samples += other.latency_samples;
    }

    void set_enable_latency_samples(bool val) {
        lock.lock();
        enable_latency_samples = val;
        lock.unlock();
    }

public:
    spinlock_t lock;
};

struct op_t {

    op_t(query_stats_t *_stats) : stats(_stats) { }
    virtual ~op_t() { }

    void push_stats(float latency, int count) {
        stats->push(latency, count);
    }

    query_stats_t *stats;

    virtual void start() = 0;

    virtual bool end_maybe() = 0;

    virtual void end() = 0;
};

/* op_ts previously existed in this weird middle ground between classes and
 * functors. Subclasses were generally only manipulated through abstract
 * pointers of type op_t thus the only visible method was run(). Furthermore
 * run was called multiple times on the same op_t without ever reallocating it.
 * This was nice for performance reasons but confusing. This had to be changed
 * for pipelining since state now had to be saved accross multiple calls. This
 * is the justification for the op generator type which generates a new
 * instance of the op with its own state that can be put in the pipeline queue.
 * */
struct op_generator_t {
    /* this method follows the creation paradigm. The caller is responsible for
     * deleting the op_t * that is returned. */
    virtual op_t *generate() = 0;
    query_stats_t query_stats;
};

#endif /* __OP_HPP__ */
