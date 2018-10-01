/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>

// time in seconds
const uint32_t ONE_MINUTE = 60;
const uint32_t ONE_HOUR = ONE_MINUTE * 60;
const uint32_t ONE_DAY = ONE_HOUR * 24;
const uint32_t ONE_YEAR = ONE_DAY * 365;

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   class token : public contract {
      public:
         token( account_name self ):contract(self){}

         void create( account_name issuer,
                      asset        maximum_supply);

         void issue( account_name to, asset quantity, string memo );

         void transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo );

         void addstake( account_name staker,
                        asset        quantity,
                        uint32_t     duration );

         void updatestakes( string symbolname );

         inline asset get_supply( symbol_name sym )const;

         inline asset get_balance( account_name owner, symbol_name sym )const;

         inline int64_t get_stake_weight( uint32_t stake_duration )const;

      private:
         struct account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.name(); }
         };

         struct currency_stats {
            asset                   supply;
            asset                   max_supply;
            account_name            issuer;

            uint64_t primary_key()const { return supply.symbol.name(); }
         };

         struct stake {
            uint64_t                id; // use available_primary_key() to generate
            asset                   quantity;
            eosio::time_point_sec   start;
            uint32_t                duration;

            uint64_t primary_key()const { return id; }
         };

         struct stake_stat {
            account_name   staker;
            asset          total_stake;
            int64_t        stake_weight;

            uint64_t primary_key()const { return staker; }
         };

         typedef eosio::multi_index<N(accounts), account> accounts;
         typedef eosio::multi_index<N(stat), currency_stats> stats;
         typedef eosio::multi_index<N(stakes), stake> stakes;
         typedef eosio::multi_index<N(stakestats), stake_stat> stake_stats;

         void sub_balance( account_name owner, asset value,  bool no_fee=false );
         void add_balance( account_name owner, asset value, account_name ram_payer );

         asset get_stake( account_name owner, eosio::symbol_type sym )const;
         int64_t get_stake_weight( account_name owner, eosio::symbol_type sym )const;
         asset get_unstaked_balance( account_name owner, eosio::symbol_type sym )const;
         int64_t distribute( asset quantity );

         const float transaction_fee = 0.01; // 1%

         const float transaction_fee_to_stakers = 0.7f; // 70%
         //const float transaction_fee_to_likes = 0.0f;
         // inSpace gets the rest
         const account_name inspace_account = N(inspace);

         static const size_t stake_count = 5;
         // short durations for testing
         // TODO: change to days, not minutes
         const uint32_t stake_durations[stake_count] = {
            0,
            30 * ONE_MINUTE,
            90 * ONE_MINUTE,
            180 * ONE_MINUTE,
            360  * ONE_MINUTE
         };

         const int64_t stake_weights[stake_count] = {
            0,
            5,
            6,
            7,
            10
         };

         const uint32_t update_interval = ONE_MINUTE;

      public:
         struct transfer_args {
            account_name  from;
            account_name  to;
            asset         quantity;
            string        memo;
         };
   };

   asset token::get_supply( symbol_name sym )const
   {
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );
      return st.supply;
   }

   asset token::get_balance( account_name owner, symbol_name sym )const
   {
      accounts accountstable( _self, owner );
      const auto& ac = accountstable.get( sym );
      return ac.balance;
   }

   int64_t token::get_stake_weight( uint32_t stake_duration )const
   {
      size_t i = 0;
      while ((i+1) < stake_count && stake_duration >= stake_durations[i+1]) {
         i = i+1;
      }
      return stake_weights[i];
   }


} /// namespace eosio
