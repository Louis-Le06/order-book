#pragma once

#include <cstdint>

namespace ob {

using OrderId  = std::uint64_t;
using Price    = std::int64_t;   // price in ticks, 1 tick = 0.01 (avoids floating-point errors)
using Quantity = std::uint64_t;

enum class Side { Buy, Sell };
enum class OrderType { Limit, Market };

struct Order {
    OrderId   id;
    Side      side;
    OrderType type;
    Price     price;     // ignored for market orders
    Quantity  quantity;  // remaining (unfilled) quantity
};

struct Trade {
    OrderId  buyOrderId;
    OrderId  sellOrderId;
    Price    price;      // execution price = price of the resting (maker) order
    Quantity quantity;
};

inline const char* toString(Side side) { return side == Side::Buy ? "BUY" : "SELL"; }

}  // namespace ob
