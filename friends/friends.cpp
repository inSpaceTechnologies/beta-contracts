#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>

using namespace eosio;
using namespace std;

class friends : public contract {
   using contract::contract;

   public:

      friends(account_name self) :
         contract(self),
         request_table(_self, _self),
         friendship_table(_self, _self) {}

      // @abi action
      void addrequest(account_name user, account_name to) {
         require_auth(user);

         /** make sure a friendship doesn't already exist **/
         auto friendships_by_account1 = friendship_table.get_index<N(by_account1)>();
         bool exists = false;
         for (auto iterator = friendships_by_account1.find(user); iterator != friendships_by_account1.end(); ++iterator) {
            if ((*iterator).account2 == to) {
               exists = true;
               break;
            }
         }
         eosio_assert(!exists, "Friendship exists!");

         exists = false;
         for (auto iterator = friendships_by_account1.find(to); iterator != friendships_by_account1.end(); ++iterator) {
            if ((*iterator).account2 == user) {
               exists = true;
               break;
            }
         }
         eosio_assert(!exists, "Friendship exists!");

         /**  make sure request doesn't already exist **/
         auto requests_by_from = request_table.get_index<N(by_from)>();
         exists = false;
         for (auto iterator = requests_by_from.find(user); iterator != requests_by_from.end(); ++iterator) {
            if ((*iterator).to == to) {
               exists = true;
               break;
            }
         }
         eosio_assert(!exists, "Friend request exists!");

         /**  check for opposite request **/
         exists = false;
         auto iterator = requests_by_from.find(to);
         for (; iterator != requests_by_from.end(); ++iterator) {
            if ((*iterator).to == user) {
               exists = true;
               break;
            }
         }
         if (exists) {
            /** delete it **/
            requests_by_from.erase(iterator);

            /** add friendship **/
            friendship_table.emplace(_self, [&](auto& friendship_record) {
               friendship_record.id = request_table.available_primary_key();
               friendship_record.account1 = user;
               friendship_record.account2 = to;
            });

            /** don't add the request **/
            return;
         }

         /** add the record **/
         request_table.emplace(_self, [&](auto& request_record) {
            request_record.id = request_table.available_primary_key();
            request_record.from = user;
            request_record.to = to;
         });
      }

   private:

      /*

      data structures for tables

      */
      // @abi table requests
      struct request_record {
         uint64_t id;
         account_name from;
         account_name to;

         auto primary_key() const { return id; }
         account_name get_from() const { return from; }
         account_name get_to() const { return to; }

         EOSLIB_SERIALIZE(request_record, (id)(from)(to))
      };

      // @abi table friendships
      struct friendship_rec { // 'friendship_record' is longer than 13 characters
         uint64_t id;
         account_name account1;
         account_name account2;

         auto primary_key() const { return id; }
         account_name get_account1() const { return account1; }
         account_name get_account2() const { return account2; }

         EOSLIB_SERIALIZE(friendship_rec, (id)(account1)(account2))
      };

      /*

      multi-index tables

      */
      typedef multi_index<N(requests),
                          request_record,
                          indexed_by<N(by_from), /** secondary index on from **/
                                     const_mem_fun<request_record, account_name, &request_record::get_from>
                                    >,
                          indexed_by<N(by_to), /** secondary index on to **/
                                     const_mem_fun<request_record, account_name, &request_record::get_to>
                                    >
                         > request_table_type;

      typedef multi_index<N(friendships),
                          friendship_rec,
                          indexed_by<N(by_account1),
                                     const_mem_fun<friendship_rec, account_name, &friendship_rec::get_account1>
                                    >,
                          indexed_by<N(by_account2),
                                     const_mem_fun<friendship_rec, account_name, &friendship_rec::get_account2>
                                    >
                          > friendship_table_type;

      request_table_type request_table;
      friendship_table_type friendship_table;

};

EOSIO_ABI(friends, (addrequest))
