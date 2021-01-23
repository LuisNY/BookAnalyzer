#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <unordered_map>
#include <iomanip>

/*
This implementation keeps 3 separate data structures. 
One map for the Buy orders, that will keep the items ordered by price in descending order;
One map for the Sell orders, that will keep the items ordered by price in ascending order;
The third is a hash table, where we store all the orders id (as long as there is some size left on mkt) and the corresponding price and Side
We can look up the order by id in the hash table (constant time access), and given the price of that order we can go into the Buy or Sell Maps 
and look for the price in that map (time complexity O(logn)). 

When an order needs to be reduces, we lookup the id in the hash table, then find the corresponding item in the appropriate map, and finally we reduce the size.
If the size becomes 0, then we remove the information about that order id from both data structures.

The input of this program is a file, and the file name is specified in the main itself, together with the target.
The output of this program is simply printed to stdout.
*/

enum Side {
    BUY = 0,
    SELL,
    UNKNOWN
};


class BookAnalyzer 
{
    struct comparatorBuy {
        bool operator()(const double& a, const double& b)
        {
            return a > b;
        }
    };

    struct comparatorSell {
        bool operator()(const double& a, const double& b)
        {
            return a < b;
        }
    };

public:

    BookAnalyzer(int target) : target_(target), totBuySize_(0), totSellSize_(0), prevExpenses_(0), prevIncome_(0), prevNanExp_(true), prevNanIncome_(true) 
    {   }

    int target_;
    int totBuySize_;
    int totSellSize_;
    double prevExpenses_;
    bool prevNanExp_;
    double prevIncome_;
    bool prevNanIncome_;

    //keep items ordered by price, so that we can always get the next min/max available
    //for each price we store a map with all the orders distinct by id, and corresponding size
    //map <price : un_map <id : size> >
    std::map<double, std::unordered_map<std::string, int>, comparatorBuy> buyMap_; 
    std::map<double, std::unordered_map<std::string, int>, comparatorSell> sellMap_;
    
    //also keep all orders id in hash table, for each id we store the side (to pick the proper map) and the price, to find the element in the buy/sell maps
    //map <id : <side, price> >
    std::unordered_map<std::string, std::pair<Side, double>> hashTable_;


    void handleNewOrder(const std::string& id, const Side side, const int size, const double price, const long timestamp) 
    {
        if(side == Side::BUY)
            this->handleNewBuyOrder(size, price, id, timestamp);
        else if (side == Side::SELL)
            this->handleNewSellOrder(size, price, id, timestamp);

        hashTable_.insert(std::make_pair(id, std::make_pair(side, price))); //add id to hashmap if it doesn't exist
    }

    void reduceOrder(const std::string& id, const Side side, const int size, const long timestamp)
    {
        auto hashElem = hashTable_.find(id);
        auto price = hashElem->second.second;
        bool removeFromMemory = false;

        if (side == Side::BUY)
            reduceBuyOrder(hashElem, price, size, timestamp, removeFromMemory);
        else if (side == Side::SELL)
            reduceSellOrder(hashElem, price, size, timestamp, removeFromMemory);
        else 
        {
            //ignore, unknown order type
        }

        if (removeFromMemory)
            hashTable_.erase(id); //remove order id from hashtable since there is no remaining size on market
    }

private:

    std::unordered_map<std::string, int> createNewInnerMap(const std::string id, const int size) 
    {
        std::unordered_map<std::string, int> newMap;
        newMap.emplace(std::make_pair(id, size)); 
        return newMap;
    }

    void searchInnerMap(std::unordered_map<std::string, int> innerMap, double price, long timestamp, double& prevOut, double& amount, int& currSize)
    {
        int localSize=0;
        auto innerMapIter = innerMap.begin();
        while(localSize<(target_-currSize) && innerMapIter!= innerMap.end())
        {
            localSize += innerMapIter->second;
            ++innerMapIter;
        }
        
        if(localSize>(target_-currSize))
            localSize = target_-currSize;

        amount += localSize * price;
        currSize += localSize;
    }

    void printNA(const long timestamp, bool& prevNan, Side side)
    {
        prevNan = true;
        std::cout << timestamp << (side == Side::BUY ? " S" : " B") << " NA" << std::endl; 
    }

    void print(const double& amount, double& prevAmount, bool& prevIsNan, const long timestamp, const Side side)
    {
        if (amount != prevAmount || prevIsNan == true)
            std::cout << timestamp << " " << (side == Side::BUY ? "S" :"B") << " " << amount << std::endl;

        prevAmount = amount;
        prevIsNan = false;
    }

    void printBuy(long timestamp)
    {
        int currSize=0;
        double income = 0;
        auto it = buyMap_.begin();

        while(currSize<target_ && it!=buyMap_.end())
        {
            this->searchInnerMap(it->second, it->first, timestamp, prevIncome_, income, currSize);
            ++it;
        }

        this->print(income, prevExpenses_, prevNanExp_, timestamp, Side::BUY);
    }

    void printSell(long timestamp)
    {
        int currSize=0;
        double expenses = 0;
        auto it = sellMap_.begin();

        while(currSize<target_ && it!=sellMap_.end())
        {
            this->searchInnerMap(it->second, it->first, timestamp, prevExpenses_, expenses, currSize);
            ++it;
        }

        this->print(expenses, prevIncome_, prevNanIncome_, timestamp, Side::SELL);
    }
    
    void handleNewBuyOrder(const int size, const double price, const std::string id, const long timestamp)
    {
        totBuySize_ += size;
        auto buyIter = buyMap_.find(price);

        if (buyIter != buyMap_.end())
            buyIter->second.insert(std::make_pair(id, size));
        else 
            buyMap_.emplace(std::make_pair(price, createNewInnerMap(id, size)));

        if (target_<=totBuySize_)
            this->printBuy(timestamp);
    }

    void handleNewSellOrder(const int size, const double price, const std::string id, long timestamp)
    {
        totSellSize_ += size;
        auto sellIter = sellMap_.find(price);

        if (sellIter != sellMap_.end())
            sellIter->second.insert(std::make_pair(id, size));
        else 
            sellMap_.emplace(std::make_pair(price, this->createNewInnerMap(id, size)));

        if (target_<=totSellSize_)
            this->printSell(timestamp);
    }

    bool searchId(std::unordered_map<std::string, int>& innerMap, const std::string& id, const int size, bool& removeFromMemory, int& totSize, const Side side)
    {
        auto idIter = innerMap.find(id);
        if (idIter != innerMap.end())
        {
            idIter->second -= size;

            if (idIter->second <= 0)
            {
                innerMap.erase(id);

                if (innerMap.empty())
                    removeFromMemory = true;
            }

            totSize -= size;
            return true;
        }

        return false;
    }

    void reduceBuyOrder(std::unordered_map<std::string, std::pair<Side, double>>::iterator& hashElem, const double price, const int size, const long timestamp, bool& removeFromMemory)
    {
         auto iter = buyMap_.find(price);

        if(iter != buyMap_.end())
        {
            if (this->searchId(iter->second, hashElem->first, size, removeFromMemory, totBuySize_, Side::BUY)) // look for the order to reduce by id
            {
                if (target_ <= totBuySize_)
                    this->printBuy(timestamp);
                else if (target_ > totBuySize_ && prevNanExp_ == false)
                    this->printNA(timestamp, prevNanExp_, Side::BUY);
            }
        }

        if (removeFromMemory)
            buyMap_.erase(price);
    }

    void reduceSellOrder(std::unordered_map<std::string, std::pair<Side, double>>::iterator& hashElem, const double& price, const int size, const long timestamp, bool& removeFromMemory)
    {
        auto iter = sellMap_.find(price);

        if(iter != sellMap_.end())
        {
            if (this->searchId(iter->second, hashElem->first, size, removeFromMemory, totSellSize_, Side::SELL))
            {
                if (target_<= totSellSize_)
                    this->printSell(timestamp);
                else if (target_ > totSellSize_ && prevNanIncome_ == false)
                    this->printNA(timestamp, prevNanIncome_, Side::SELL);
            }
        }

        if (removeFromMemory)
            sellMap_.erase(price);
    }
};

int main() 
{
    std::cout << std::setprecision(2) << std::fixed;
    int target = 200;
    BookAnalyzer bookAnalyzer = BookAnalyzer(target);

    std::ifstream infile("input.in");

    long timestamp;
    char type;
    std::string id;
    Side side;
    double price;
    int size;
    std::string line;
    
    while (std::getline(infile, line)) //process line by line until end of file
    {
        std::istringstream iss(line);
        if (!(iss >> timestamp >> type)) 
            break;

        if (type == 'A') //if new order process it
        {
            char tempSide;
            iss >> id >> tempSide >> price >> size;

            side = tempSide == 'B' ? Side::BUY : Side::SELL;

            bookAnalyzer.handleNewOrder(id, side, size, price, timestamp);
            
        }
        else if (type == 'R') //else reduce existing order
        {
            iss >> id >> size;

            auto hashElem = bookAnalyzer.hashTable_.find(id);

            if (hashElem != bookAnalyzer.hashTable_.end())
            {
                Side side = hashElem->second.first;
                bookAnalyzer.reduceOrder(id, side, size, timestamp);
            }
            else 
            {
                //ignore, order id not found
            }
        }
    }

    return 0;
}