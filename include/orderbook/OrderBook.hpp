#pragma once

#include "orderbook/Types.hpp"

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ob {

/// Aggregated view of one price level.
struct LevelInfo {
    Price       price;
    Quantity    quantity;    // total resting quantity at this price
    std::size_t orderCount;  // number of orders queued at this price
};

struct BookSnapshot {
    std::vector<LevelInfo> bids;  // best (highest) first
    std::vector<LevelInfo> asks;  // best (lowest) first
};

/// Limit order book with price-time priority matching.
///
/// - Bids are kept in descending price order, asks in ascending price order.
/// - Orders at the same price are filled FIFO (first come, first served).
/// - Trades execute at the resting (maker) order's price.
/// - Cancel is O(1) thanks to an id -> list-iterator index.
class OrderBook {
public:
    /// Adds a limit order. Matches against the opposite side first; any
    /// remaining quantity rests in the book.
    /// @throws std::invalid_argument on duplicate id, zero quantity or non-positive price.
    std::vector<Trade> addLimitOrder(OrderId id, Side side, Price price, Quantity quantity);

    /// Adds a market order. It takes liquidity at any price; whatever cannot
    /// be filled immediately is discarded (never rests in the book).
    /// @throws std::invalid_argument on duplicate id or zero quantity.
    std::vector<Trade> addMarketOrder(OrderId id, Side side, Quantity quantity);

    /// Cancels a resting order. Returns false if the id is unknown
    /// (never existed, already filled, or already cancelled).
    bool cancelOrder(OrderId id);

    [[nodiscard]] std::optional<Price> bestBid() const;
    [[nodiscard]] std::optional<Price> bestAsk() const;
    [[nodiscard]] bool contains(OrderId id) const;
    [[nodiscard]] std::size_t orderCount() const { return index_.size(); }

    /// Returns up to `depth` price levels on each side.
    [[nodiscard]] BookSnapshot snapshot(std::size_t depth = 10) const;

private:
    using OrderQueue = std::list<Order>;  // stable iterators -> O(1) erase
    using Bids = std::map<Price, OrderQueue, std::greater<Price>>;
    using Asks = std::map<Price, OrderQueue, std::less<Price>>;

    struct Location {
        Side                 side;
        Price                price;
        OrderQueue::iterator it;
    };

    template <typename Book>
    void match(Order& incoming, Book& oppositeSide, std::vector<Trade>& trades);

    template <typename Book>
    void rest(const Order& order, Book& sameSide);

    template <typename Book>
    void removeFromLevel(Book& book, const Location& loc);

    template <typename Book>
    static std::vector<LevelInfo> collectLevels(const Book& book, std::size_t depth);

    void validateNewOrder(OrderId id, Quantity quantity) const;

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, Location> index_;
};

}  // namespace ob
