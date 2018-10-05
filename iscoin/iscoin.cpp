/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "iscoin.hpp"

#include <eosiolib/print.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/transaction.hpp>
#include <string>
#include <vector>

namespace eosio {

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    // no transaction fee for issuer
    bool is_issuer = false;
    if (from == st.issuer) {
       is_issuer = true;
    }

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    sub_balance( from, quantity, is_issuer );
    add_balance( to, quantity, from );
}

void token::addstake( account_name staker,
                      asset        quantity,
                      uint32_t     duration )
{
    require_auth( staker );
    eosio_assert( is_account( staker ), "staker account does not exist");

    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must stake positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    const asset unstaked_balance = get_unstaked_balance(staker, quantity.symbol);
    eosio_assert( quantity.amount <= unstaked_balance.amount, "overdrawn unstaked balance" );

    stakes staker_stakes( _self, staker );
    staker_stakes.emplace(_self, [&](auto& s) {
      s.id = staker_stakes.available_primary_key();
      s.quantity = quantity;
      s.start = eosio::time_point_sec(now());
      s.duration = duration;
   });

   int64_t weight = get_stake_weight(duration) * quantity.amount;

   stake_stats stake_stats_table( _self, sym );
   const auto staker_stake_stats = stake_stats_table.find( staker );
   if( staker_stake_stats == stake_stats_table.end() ) {
      stake_stats_table.emplace( _self, [&]( auto& s ){
         s.staker = staker;
         s.total_stake = quantity;
         s.stake_weight = weight;
      });
   } else {
      stake_stats_table.modify( staker_stake_stats, _self, [&]( auto& s ) {
         s.total_stake += quantity;
         s.stake_weight += weight;
      });
   }
}

void token::updatestakes( string symbolname ) {
   eosio::symbol_type symbol(eosio::string_to_symbol(4, symbolname.c_str()));
   stake_stats stake_stats_table( _self, symbol.name() );

   // iterate through stake stats
   // (all stakes will have an entry because addstake adds one)
   auto iterator = stake_stats_table.begin();
   while ( iterator != stake_stats_table.end() ) {

      const auto& st = (*iterator);
      // iterate through the staker's stakes
      stakes stakestable( _self, st.staker );

      asset total_stake;
      total_stake.symbol = symbol;
      total_stake.amount = 0;

      int64_t stake_weight = 0;

      const eosio::time_point_sec currentTime(now());
      auto stake_iterator = stakestable.begin();
      while(stake_iterator != stakestable.end()) {
         const auto& stk = (*stake_iterator);
         if (stk.quantity.symbol != symbol) {
            ++stake_iterator;
            continue;
         }
         const eosio::time_point_sec expiryTime = stk.start + stk.duration;
         if (expiryTime <= currentTime) {
            // stake has expired. remove it.
            stake_iterator = stakestable.erase(stake_iterator);
         } else {
            total_stake.amount += stk.quantity.amount;

            int64_t weight = get_stake_weight(stk.duration) * stk.quantity.amount;
            stake_weight += weight;

            ++stake_iterator;
         }
      }

      if (total_stake.amount == 0) {
         // all stakes have expired.
         // remove entry
         iterator = stake_stats_table.erase(iterator);
      } else {
         // update stake stats
         stake_stats_table.modify( iterator, _self, [&]( auto& s ) {
            s.total_stake = total_stake;
            s.stake_weight = stake_weight;
         });
         ++iterator;
      }
   }

   // schedule a transaction to do it again
   eosio::transaction out;
   out.actions.emplace_back(
      permission_level{_self, N(active)},
      _self,
      N(updatestakes),
      std::make_tuple(symbolname));
   out.delay_sec = update_interval;
   out.send(_self + now(), _self); // needs a unique sender id so append current time
}

void token::sub_balance( account_name owner, asset value, bool no_fee ) {
   accounts from_acnts( _self, owner );

    eosio::symbol_type symbol = value.symbol;

   const auto& from = from_acnts.get( symbol.name(), "no balance object found" );

   const asset stake = get_stake(owner, symbol );

   const int64_t value_amount = value.amount;
   const int64_t transaction_fee_amount = no_fee ? 0 : (int64_t)(value_amount * transaction_fee);
   const int64_t total_amount = value_amount + transaction_fee_amount;

   eosio_assert( from.balance.amount - stake.amount >= total_amount, "overdrawn unstaked balance" );

   if( from.balance.amount == total_amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance.amount -= total_amount;
      });
   }

   if (no_fee) {
      return;
   }

   int64_t transaction_fee_remaining = transaction_fee_amount;
   const int64_t transaction_fee_stakers_amount = (int64_t)(transaction_fee_to_stakers * transaction_fee_amount);
   asset transaction_fee_stakers_asset;
   transaction_fee_stakers_asset.symbol = symbol;
   transaction_fee_stakers_asset.amount = transaction_fee_stakers_amount;

   transaction_fee_remaining -= distribute(transaction_fee_stakers_asset);

   const int64_t transaction_fee_likes_amount = (int64_t)(transaction_fee_to_likes * transaction_fee_amount);
   asset transaction_fee_likes_asset;
   transaction_fee_likes_asset.symbol = symbol;
   transaction_fee_likes_asset.amount = transaction_fee_likes_amount;

   transaction_fee_remaining -= distribute_likes(transaction_fee_likes_asset);

   const int64_t transaction_fee_inspace_amount = transaction_fee_remaining;
   if (transaction_fee_inspace_amount > 0) {
      asset transaction_fee_inspace_asset;
      transaction_fee_inspace_asset.symbol = symbol;
      transaction_fee_inspace_asset.amount = transaction_fee_inspace_amount;
      add_balance(inspace_account, transaction_fee_inspace_asset, _self);
   }
}

void token::add_balance( account_name owner, asset value, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

asset token::get_stake( account_name staker, eosio::symbol_type sym )const
{
   stake_stats stake_stats_table( _self, sym );
   const auto staker_stake_stats = stake_stats_table.find( staker );
   if( staker_stake_stats == stake_stats_table.end() ) {
      // no enty, so no stakes
      asset ret;
      ret.symbol = sym;
      ret.amount = 0;
      return ret;
   } else {
      return (*staker_stake_stats).total_stake;
   }
}

int64_t token::get_stake_weight( account_name staker, eosio::symbol_type sym )const
{
   stake_stats stake_stats_table( _self, sym );
   const auto staker_stake_stats = stake_stats_table.find( staker );
   if( staker_stake_stats == stake_stats_table.end() ) {
      // no enty, so no stakes
      return (int64_t)0;
   } else {
      return (*staker_stake_stats).stake_weight;
   }
}

asset token::get_unstaked_balance( account_name owner, eosio::symbol_type sym )const
{
   const asset balance = get_balance(owner, sym.name());
   const asset stake = get_stake(owner, sym);
   return asset(balance.amount - stake.amount, sym);
}

// distributes the quantity amongst stakers by stake weight.
// returns the actual amount distruted.
int64_t token::distribute( asset quantity )
{
   stake_stats stake_stats_table( _self, quantity.symbol.name() );

   std::vector<account_name>  stakers;
   std::vector<int64_t>       weights;
   int64_t                    total_weight = 0;

   // iterate through stake stats
   auto iterator = stake_stats_table.begin();
   while ( iterator != stake_stats_table.end() ) {

      const auto& st = (*iterator);

      stakers.push_back(st.staker);
      weights.push_back(st.stake_weight);
      total_weight += st.stake_weight;

      ++iterator;
   }

   if (total_weight == 0) {
      return 0;
   }

   int64_t amount_distributed = 0;

   for(size_t i = 0; i < stakers.size(); i++) {
      account_name staker = stakers[i];

      int64_t staker_weight = weights[i];

      float proportion = (float)staker_weight / total_weight;

      int64_t amount_for_staker = (int64_t)(quantity.amount  * proportion);

      asset amount_asset;
      amount_asset.symbol = quantity.symbol;
      amount_asset.amount = amount_for_staker;

      add_balance( staker, amount_asset, _self);
      amount_distributed += amount_for_staker;
   }

   return amount_distributed;
}

// distributes the quantity amongst likes by stake weight.
// returns the actual amount distruted.
int64_t token::distribute_likes( asset quantity )
{
   like_table_type like_table(N(filespace), N(filespace));
   stake_stats stake_stats_table( _self, quantity.symbol.name() );

   std::map<account_name, int64_t> liked_weights;
   int64_t total_weight = 0;

   // iterate through likes
   for (auto iterator = like_table.begin(); iterator != like_table.end(); ++iterator) {

      const account_name liked = (*iterator).liked;

      // get stake weight of liker
      const auto staker_stake_stats = stake_stats_table.find( (*iterator).liker );

      if (staker_stake_stats == stake_stats_table.end()) {
         // no stake
         liked_weights[liked] = 0;
         continue;
      }

      const int64_t likerWeight = (*staker_stake_stats).stake_weight;

      if (liked_weights.count(liked) == 0) {
         liked_weights[liked] = likerWeight;
      } else {
         liked_weights[liked] += likerWeight;
      }

      total_weight += likerWeight;
   }

   if (total_weight == 0) {
      return 0;
   }

   int64_t amount_distributed = 0;

   for (std::map<account_name, int64_t>::iterator iterator = liked_weights.begin(); iterator != liked_weights.end(); ++iterator) {

      account_name liked = iterator->first;
      int64_t weight = iterator->second;

      float proportion = (float)weight / total_weight;

      int64_t amount_for_liked = (int64_t)(quantity.amount  * proportion);

      asset amount_asset;
      amount_asset.symbol = quantity.symbol;
      amount_asset.amount = amount_for_liked;

      add_balance( liked, amount_asset, _self);
      amount_distributed += amount_for_liked;
   }

   return amount_distributed;
}

} /// namespace eosio

EOSIO_ABI( eosio::token, (create)(issue)(transfer)(addstake)(updatestakes) )
