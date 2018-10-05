#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>

using namespace eosio;
using namespace std;

static const uint64_t NULL_ID = 0;

class filespace : public contract {
   using contract::contract;

   public:

      filespace(account_name self) : contract(self) {}

      // @abi action
      void addfolder(account_name user, uint64_t id, string name, uint64_t parent_folder) {
         require_auth(user);

         /** user's scope **/
         folder_table_type folder_table(_self, user);

         /** check whether the id exists **/
         auto iterator = folder_table.find(id);
         eosio_assert(iterator == folder_table.end(), "Folder id exists!");

         /** check whether parent exists **/
         if (parent_folder != NULL_ID) {
            iterator = folder_table.find(parent_folder);
            eosio_assert(iterator != folder_table.end(), "Parent folder does not exist!");
         }

         /** add the record **/
         folder_table.emplace(_self, [&](auto& folder_record) {
            folder_record.id = id;
            folder_record.name = name;
            folder_record.parent_folder = parent_folder;
         });
      }

      // @abi action
      void renamefolder(account_name user, uint64_t id, string new_name) {
         require_auth(user);

         folder_table_type folder_table(_self, user);

         /** make sure the id exists **/
         auto iterator = folder_table.find(id);
         eosio_assert(iterator != folder_table.end(), "Folder id does not exist!");

         /** modify the record **/
         folder_table.modify(iterator, _self, [&](auto& folder_record) {
            folder_record.name = new_name;
         });
      }

      // @abi action
      void movefolder(account_name user, uint64_t id, uint64_t new_parent_folder) {
         require_auth(user);

         folder_table_type folder_table(_self, user);

         /** make sure the new parent exists **/
         if (new_parent_folder != NULL_ID) {
            auto iterator = folder_table.find(new_parent_folder);
            eosio_assert(iterator != folder_table.end(), "Parent folder does not exist!");
         }

         /** make sure the id exists **/
         auto iterator = folder_table.find(id);
         eosio_assert(iterator != folder_table.end(), "Folder id does not exist!");

         /** modify the record **/
         folder_table.modify(iterator, _self, [&](auto& folder_record) {
            folder_record.parent_folder = new_parent_folder;
         });
      }

      // @abi action
      void addfile(account_name user, uint64_t id, string name, uint64_t parent_folder, uint64_t current_version) {
         require_auth(user);

         file_table_type file_table(_self, user);
         folder_table_type folder_table(_self, user);

         /** check whether the id exists **/
         auto iterator = file_table.find(id);
         eosio_assert(iterator == file_table.end(), "File id exists!");

         /** check whether parent exists **/
         if (parent_folder != NULL_ID) {
         auto iterator = folder_table.find(parent_folder);
            eosio_assert(iterator != folder_table.end(), "Parent folder does not exist!");
         }

         /** make sure the version is valid **/
         eosio_assert(version_valid(user, current_version, id), "Version is not valid!");

         /** add the record **/
         file_table.emplace(_self, [&](auto& file_record) {
            file_record.id = id;
            file_record.name = name;
            file_record.parent_folder = parent_folder;
            file_record.current_version = current_version;
         });
      }

      // @abi action
      void renamefile(account_name user, uint64_t id, string new_name) {
         require_auth(user);

         file_table_type file_table(_self, user);

         /** make sure the id exists **/
         auto iterator = file_table.find(id);
         eosio_assert(iterator != file_table.end(), "File id does not exist!");

         /** modify the record **/
         file_table.modify(iterator, _self, [&](auto& file_record) {
            file_record.name = new_name;
         });
      }

      // @abi action
      void movefile(account_name user, uint64_t id, uint64_t new_parent_folder) {
         require_auth(user);

         file_table_type file_table(_self, user);
         folder_table_type folder_table(_self, user);

         /** make sure the new parent exists **/
         if (new_parent_folder != NULL_ID) {
            auto iterator = folder_table.find(new_parent_folder);
            eosio_assert(iterator != folder_table.end(), "Parent folder does not exist!");
         }

         /** make sure the id exists **/
         auto iterator = file_table.find(id);
         eosio_assert(iterator != file_table.end(), "File id does not exist!");

         /** modify the record **/
         file_table.modify(iterator, _self, [&](auto& file_record) {
            file_record.parent_folder = new_parent_folder;
         });
      }

      /** would be 'setcurrentversion' without the length limit **/
      // @abi action
      void setcurrentve(account_name user, uint64_t id, uint64_t new_current_version) {
         require_auth(user);

         file_table_type file_table(_self, user);

         /** make sure the id exists **/
         auto iterator = file_table.find(id);
         eosio_assert(iterator != file_table.end(), "File id does not exist!");

         /** make sure the version is valid **/
         eosio_assert(version_valid(user, new_current_version, id), "Version is not valid!");

         /** modify the record **/
         file_table.modify(iterator, _self, [&](auto& file_record) {
            file_record.current_version = new_current_version;
         });
      }

      // @abi action
      void addversion(account_name user, uint64_t id, string ipfs_hash, string sha256, uint64_t date, uint64_t file) {
         require_auth(user);

         file_table_type file_table(_self, user);
         version_table_type version_table(_self, user);

         /** check whether the id exists **/
         auto iterator = version_table.find(id);
         eosio_assert(iterator == version_table.end(), "Version id exists!");

         /** check whether file exists **/
         if (file != NULL_ID) {
           auto iterator = file_table.find(file);
           eosio_assert(iterator != file_table.end(), "File does not exist!");
         }

         /** add the record **/
         version_table.emplace(_self, [&](auto& version_record) {
             version_record.id = id;
             version_record.ipfs_hash = ipfs_hash;
             version_record.sha256 = sha256;
             version_record.date = date;
             version_record.file = file;
         });
      }

      // @abi action
      void addlike(account_name user, uint64_t id, account_name liked, uint64_t version) {
         like_table_type like_table(_self, _self);

         require_auth(user);

         /** check whether the id exists **/
         auto like_iterator = like_table.find(id);
         eosio_assert(like_iterator == like_table.end(), "Like id exists!");

         /** check whether version exists **/
         version_table_type version_table(_self, liked);
         auto version_iterator = version_table.find(version);
         eosio_assert(version_iterator != version_table.end(), "Version does not exist!");

         /** add the record **/
         like_table.emplace(_self, [&](auto& like_record) {
             like_record.id = id;
             like_record.liker = user;
             like_record.liked = liked;
             like_record.version = version;
         });
      }

      // @abi action
      void deletefolder(account_name user, uint64_t id) {
         require_auth(user);

         folder_table_type folder_table(_self, user);
         file_table_type file_table(_self, user);

         /** get the folder and make sure it exists **/
         auto iterator = folder_table.find(id);
         eosio_assert(iterator != folder_table.end(), "Folder id does not exist!");

         /** make sure there are no child folders **/
         auto folders_by_parent = folder_table.get_index<N(by_parent)>();
         auto folder_iterator = folders_by_parent.find(id);
         eosio_assert(folder_iterator == folders_by_parent.end(), "Folder is not empty!");

         /** make sure there are no child files **/
         auto files_by_parent = file_table.get_index<N(by_parent)>();
         auto file_iterator = files_by_parent.find(id);
         eosio_assert(file_iterator == files_by_parent.end(), "Folder is not empty!");

         /** delete the folder **/
         folder_table.erase(iterator);
      }

      // @abi action
      void deletefile(account_name user, uint64_t id) {
         require_auth(user);

         file_table_type file_table(_self, user);
         version_table_type version_table(_self, user);

         /** get the file and make sure it exists **/
         auto iterator = file_table.find(id);
         eosio_assert(iterator != file_table.end(), "File id does not exist!");

         /** delete all versions **/
         auto versions_by_file = version_table.get_index<N(by_file)>();
         auto version_iterator = versions_by_file.find(id);
         while (version_iterator != versions_by_file.end()) {
            version_iterator = versions_by_file.erase(version_iterator);
         }

         /** delete the file itself **/
         file_table.erase(iterator);
      }

      // @abi action
      void deletelike(account_name user, uint64_t id) {
         like_table_type like_table(_self, _self);

         require_auth(user);

         /** get the like and make sure it exists **/
         auto iterator = like_table.find(id);
         eosio_assert(iterator != like_table.end(), "Like id does not exist!");

         /** make sure it's the user's like **/
         eosio_assert((*iterator).liker == user, "Can't remove somebody else's like!");

         /** delete the like **/
         like_table.erase(iterator);
      }

   private:

      bool version_valid(account_name user, uint64_t id, uint64_t file) {
         if (id == NULL_ID) {
            return true;
         }

         version_table_type version_table(_self, user);

         /** check whether the id exists **/
         auto iterator = version_table.find(id);
         if (iterator == version_table.end()) {
            return false;
         }

         /** check whether the version's file is correct **/
         if ((*iterator).file != file) {
            return false;
         }

         return true;
      }

      /*

      data structures for tables

      */
      // @abi table folders
      struct folder_record {
         uint64_t id;
         string name;
         uint64_t parent_folder;

         auto primary_key() const { return id; }
         uint64_t get_parent() const { return parent_folder; }

         EOSLIB_SERIALIZE(folder_record, (id)(name)(parent_folder))
      };

      // @abi table files
      struct file_record {
         uint64_t id;
         string name;
         uint64_t parent_folder;
         uint64_t current_version;

         auto primary_key() const { return id; }
         uint64_t get_parent() const { return parent_folder; }

         EOSLIB_SERIALIZE(file_record, (id)(name)(parent_folder)(current_version))
      };

      // @abi table versions
      struct version_record {
         uint64_t id;
         string ipfs_hash;
         string sha256;
         uint64_t date;
         uint64_t file;

         auto primary_key() const { return id; }
         uint64_t get_file() const { return file; }

         EOSLIB_SERIALIZE(version_record, (id)(ipfs_hash)(sha256)(date)(file))
      };

      // @abi table likes
      struct like_record {
         uint64_t id;
         account_name liker;
         account_name liked;
         uint64_t version;

         auto primary_key() const { return id; }

         EOSLIB_SERIALIZE(like_record, (id)(liker)(liked)(version))
      };

      /*

      multi-index tables

      */
      typedef multi_index<N(folders),
                          folder_record,
                          indexed_by<N(by_parent), /** secondary index on parent **/
                                     const_mem_fun<folder_record, uint64_t, &folder_record::get_parent>
                                    >
                         > folder_table_type;

      typedef multi_index<N(files),
                          file_record,
                          indexed_by<N(by_parent), /** secondary index on parent **/
                                     const_mem_fun<file_record, uint64_t, &file_record::get_parent>
                                    >
                         > file_table_type;

      typedef multi_index<N(versions),
                          version_record,
                          indexed_by<N(by_file), /** secondary index on file **/
                                     const_mem_fun<version_record, uint64_t, &version_record::get_file>
                                     >
                         > version_table_type;

      typedef multi_index<N(likes),
                          like_record
                         > like_table_type;

};

EOSIO_ABI(filespace, (addfolder)(renamefolder)(movefolder)(addfile)(renamefile)(movefile)(setcurrentve)(addversion)(deletefolder)(deletefile)(addlike)(deletelike))
