#include "orderbook/OrderBook.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>

namespace ob {

void OrderBook::validateNewOrder(OrderId id, Quantity quantity) const {
    if (quantity == 0) {
        throw std::invalid_argument("quantity must be greater than zero");
    }
    if (index_.count(id) != 0) {
        throw std::invalid_argument("duplicate order id " + std::to_string(id));
    }
}

std::vector<Trade> OrderBook::addLimitOrder(OrderId id, Side side, Price price, Quantity quantity) {
    validateNewOrder(id, quantity);
    if (price <= 0) {
        throw std::invalid_argument("price must be greater than zero");
    }

    Order order{id, side, OrderType::Limit, price, quantity};
    std::vector<Trade> trades;

    if (side == Side::Buy) {
        match(order, asks_, trades);
        if (order.quantity > 0) rest(order, bids_);
    } else {
        match(order, bids_, trades);
        if (order.quantity > 0) rest(order, asks_);
    }
    return trades;
}

std::vector<Trade> OrderBook::addMarketOrder(OrderId id, Side side, Quantity quantity) {
    validateNewOrder(id, quantity);

    Order order{id, side, OrderType::Market, 0, quantity};
    std::vector<Trade> trades;

    if (side == Side::Buy) {
        match(order, asks_, trades);
    } else {
        match(order, bids_, trades);
    }
    // Unfilled remainder of a market order is intentionally dropped.
    return trades;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto found = index_.find(id);
    if (found == index_.end()) return false;

    const Location loc = found->second;
    if (loc.side == Side::Buy) {
        removeFromLevel(bids_, loc);
    } else {
        removeFromLevel(asks_, loc);
    }
    index_.erase(found);
    return true;
}

std::optional<Price> OrderBook::bestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::bestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

bool OrderBook::contains(OrderId id) const { return index_.count(id) != 0; }

BookSnapshot OrderBook::snapshot(std::size_t depth) const {
    return BookSnapshot{collectLevels(bids_, depth), collectLevels(asks_, depth)};
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

template <typename Book>
void OrderBook::match(Order& incoming, Book& oppositeSide, std::vector<Trade>& trades) {
    while (incoming.quantity > 0 && !oppositeSide.empty()) {
        auto levelIt = oppositeSide.begin();  // best price on the opposite side
        const Price levelPrice = levelIt->first;

        if (incoming.type == OrderType::Limit) {
            const bool crosses = (incoming.side == Side::Buy) ? levelPrice <= incoming.price
                                                              : levelPrice >= incoming.price;
            if (!crosses) break;
        }

        OrderQueue& queue = levelIt->second;
        while (incoming.quantity > 0 && !queue.empty()) {
            Order& resting = queue.front();  // oldest order at this price (time priority)
            const Quantity fill = std::min(incoming.quantity, resting.quantity);

            if (incoming.side == Side::Buy) {
                trades.push_back(Trade{incoming.id, resting.id, levelPrice, fill});
            } else {
                trades.push_back(Trade{resting.id, incoming.id, levelPrice, fill});
            }

            incoming.quantity -= fill;
            resting.quantity -= fill;

            if (resting.quantity == 0) {
                index_.erase(resting.id);
                queue.pop_front();
            }
        }

        if (queue.empty()) {
            oppositeSide.erase(levelIt);
        }
    }
}

template <typename Book>
void OrderBook::rest(const Order& order, Book& sameSide) {
    OrderQueue& queue = sameSide[order.price];
    queue.push_back(order);
    index_.emplace(order.id, Location{order.side, order.price, std::prev(queue.end())});
}

template <typename Book>
void OrderBook::removeFromLevel(Book& book, const Location& loc) {
    auto levelIt = book.find(loc.price);
    if (levelIt == book.end()) return;  // should never happen if index_ is consistent

    levelIt->second.erase(loc.it);
    if (levelIt->second.empty()) {
        book.erase(levelIt);
    }
}

template <typename Book>
std::vector<LevelInfo> OrderBook::collectLevels(const Book& book, std::size_t depth) {
    std::vector<LevelInfo> levels;
    levels.reserve(std::min(depth, book.size()));

    for (const auto& [price, queue] : book) {
        if (levels.size() >= depth) break;
        Quantity total = 0;
        for (const Order& o : queue) total += o.quantity;
        levels.push_back(LevelInfo{price, total, queue.size()});
    }
    return levels;
}

}  // namespace ob
