#include <bits/stdc++.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <cstdint>

enum class Ordertype{
    goodtillcancel,
    fillandkill
};

enum class Side{
    buy,
    sell
};

using Price = int32_t;
using Quantity = uint32_t;
using OrderID = uint64_t;

struct Levelinfo{
    Price price;
    Quantity quantity;
};

using lvlinfos = std::vector<Levelinfo>;

class Orderbooklvlinfo{
    public:
        Orderbooklvlinfo(const lvlinfos& bids, const lvlinfos& asks) : bids_ {bids}, asks_ {asks} {}
        const lvlinfos& GetBids() const { return bids_; }
        const lvlinfos& GetAsks() const { return asks_; }

    private:
        lvlinfos bids_;
        lvlinfos asks_;
};

class Order{
public:
    Order(Ordertype ordertype, OrderID orderID, Side side, Price price, Quantity quantity)
        : ordertype_ {ordertype} , orderid_ {orderID} , side_ {side}, price_ {price}, remainingQuantity {quantity} , initialQuantity{quantity}  {}
    OrderID GetOrderID() const { return orderid_; }
    Ordertype GetOrderType() const { return ordertype_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity; }
    Quantity GetFilledQuantity() const { return initialQuantity - remainingQuantity; }
    bool IsFilled() const { return GetRemainingQuantity() == 0; }
    void Fill(Quantity quantity){
        if(quantity > GetRemainingQuantity()) throw std::logic_error("ordered can't be bigger than the remaining quantity");
        remainingQuantity -= quantity;
    }
private:
    Ordertype ordertype_;
    OrderID orderid_;
    Side side_;
    Price price_;
    Quantity remainingQuantity;
    Quantity initialQuantity;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify{
public:
    OrderModify(OrderID orderID, Side side, Price price, Quantity quantity) : orderid_ {orderID}, side_ {side} , price_ {price} , quantity_ {quantity}  {}
    OrderID GetOrderID() const { return orderid_; }
    Price GetPrice() const { return price_; }
    Quantity GetQuantity() const { return quantity_; }
    Side GetSide() const { return side_; }

    OrderPointer ToOrderPointer(Ordertype type) const {
        return std::make_shared<Order>(type, GetOrderID(), GetSide(), GetPrice(), GetQuantity());
    }
private:
    OrderID orderid_;
    Side side_;
    Price price_;
    Quantity quantity_;
};

struct TradeInfo{
    OrderID orderId;
    Price price;
    Quantity quantity;
};

class Trade{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade) : bidTrade_ { bidTrade }, askTrade_ { askTrade } { }
    TradeInfo GetBidTrade() const { return bidTrade_; }
    TradeInfo GetAskTrade() const { return askTrade_; }
private:
    TradeInfo bidTrade_ , askTrade_;
};

using Trades = std::vector<Trade>;

class OrderBook{
private:
    struct OrderEntery{
        OrderPointer order_ { nullptr };
        OrderPointers::iterator location;
    };
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers> asks_;
    std::unordered_map<OrderID, OrderEntery> order_;

    bool CanMatch(Side side, Price price) const {
        if(side == Side::buy){
            if(asks_.empty()) return false;

            const auto& [BestAsk, _] = *asks_.begin();
            return (price >= BestAsk);
        }else{
            if(bids_.empty()) return false;

            const auto& [BestBid, _] = *bids_.begin();
            return (price <= BestBid);
        }
    }

    Trades MatchOrders(){
        Trades trades;
        trades.reserve(order_.size());

        while(true){
            if(bids_.empty() || asks_.empty()) break;

            auto& [askPrice, asks] = *asks_.begin();
            auto& [bidPrice, bids] = *bids_.begin();

            if(bidPrice < askPrice) break;

            while(bids.size() && asks.size()){
                auto& bid = bids.front();
                auto& ask = asks.front();

                Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
                bid->Fill(quantity);
                ask->Fill(quantity);

                if(bid->IsFilled()) {
                    bids.pop_front();
                    order_.erase(bid->GetOrderID());
                }
                if(ask->IsFilled()){
                    asks.pop_front();
                    order_.erase(ask->GetOrderID());
                }

                if(bids.empty()) bids_.erase(bidPrice);
                if(asks.empty()) asks_.erase(askPrice);

                trades.push_back(Trade {TradeInfo{bid->GetOrderID(), bid->GetPrice(), quantity}, TradeInfo{ask->GetOrderID(), ask->GetPrice(), quantity}});
            }
        }
        if(!bids_.empty()){
            auto& [_, bids] = *bids_.begin();
            auto& order = bids.front();
            if(order->GetOrderType() == Ordertype::fillandkill) CancelOrder(order->GetOrderID());
        }
        if(!asks_.empty()){
            auto& [_, asks] = *asks_.begin();
            auto& order = asks.front();
            if(order->GetOrderType() == Ordertype::fillandkill) CancelOrder(order->GetOrderID());
        }
        return trades;
    }
public:
    Trades AddOrder(OrderPointer order){
        if(order_.contains(order->GetOrderID())) return {};
        if((order->GetOrderType() == Ordertype::fillandkill) && !CanMatch(order->GetSide(), order->GetPrice())) return {};

        OrderPointers::iterator it;
        if(order->GetSide() == Side::buy){
            auto& orders = bids_[order->GetPrice()];
            orders.push_back(order);
            it = std::next(orders.begin(), orders.size()-1);
        }else{
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            it = std::next(orders.begin(), orders.size()-1);
        }

        order_.insert({order->GetOrderID(), OrderEntery{order, it}});
        return MatchOrders();
    }

    void CancelOrder(OrderID orderID){
        if(!order_.contains(orderID)) return;
        const auto& [order, orderIterator] = order_.at(orderID);
        order_.erase(orderID);

        if(order->GetSide() == Side::buy){
            Price price = order->GetPrice();
            auto& orders = bids_.at(price);
            orders.erase(orderIterator);
            if(orders.empty()) bids_.erase(price);
        }else{
            Price price = order->GetPrice();
            auto& orders = asks_.at(price);
            orders.erase(orderIterator);
            if(orders.empty()) asks_.erase(price);
        }
    }

    Trades MatchOrder(OrderModify order){
        if(!order_.contains(order.GetOrderID())) return {};
        const auto& [existingOrder, _] = order_.at(order.GetOrderID());
        CancelOrder(order.GetOrderID());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const { return order_.size(); }

    Orderbooklvlinfo getOrderInfo() const {
        lvlinfos bidInfo, askInfo;
        bidInfo.reserve(bids_.size());
        askInfo.reserve(asks_.size());

        auto GenerateLvlInfo = [] (Price price, const OrderPointers& orders){
            return Levelinfo{price , std::accumulate(orders.begin(), orders.end(), (Quantity)0 ,
                [] (Quantity runningSum, const OrderPointer& order) { return runningSum + order->GetRemainingQuantity(); }
            )};
        };

        for(const auto& [price, orders]: bids_) bidInfo.push_back(GenerateLvlInfo(price, orders));
        for(const auto& [price, orders]: asks_) askInfo.push_back(GenerateLvlInfo(price, orders));

        return Orderbooklvlinfo { bidInfo, askInfo};
    }
};

int main(){
    OrderBook book;
    const OrderID orderID = 1;
    book.AddOrder(std::make_shared<Order>(Ordertype::goodtillcancel , orderID, Side::sell, 100, 10));
    book.AddOrder(std::make_shared<Order>(Ordertype::goodtillcancel , 2, Side::sell, 100, 10));
    std::cout << book.Size() << std::endl;
    book.CancelOrder(orderID);
    book.CancelOrder(orderID);
    std::cout << book.Size() << std::endl;
    return 0;
}
