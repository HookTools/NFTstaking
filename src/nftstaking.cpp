#include <eosio/eosio.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/action.hpp>
#include <eosio/transaction.hpp>
#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <eosio/time.hpp>


using namespace eosio;
using std::string;
using std::vector;

name tokencontract = name("eosio.token");


CONTRACT nftstaking : public eosio::contract {
 public:
  nftstaking( name receiver, name code, datastream<const char*> ds ):
    contract(receiver, code, ds),
    _assetid(receiver, receiver.value),
    _config(receiver, receiver.value),
    _stake(receiver, receiver.value)
    {}


    [[eosio::on_notify("atomicassets::transfer")]]
    void transfernft(name from,name to, vector<uint64_t> asset_ids, string memo){
        if (to != get_self() || from == get_self()) return;

      auto itr_stake = _stake.find(from.value);
      if ( itr_stake == _stake.end()) {
        _stake.emplace(get_self(), [&](auto& row) {
          row.username = from;
          row.assets = asset_ids;
        });
      } else {
        vector<uint64_t> assets = itr_stake ->assets;
        for( int i = 0; i < asset_ids.size(); i++) {
          assets.push_back(asset_ids[i]);
        }
        _stake.modify(itr_stake, get_self(), [&](auto& row) {
          row.assets = assets;
        });
      }
    }

    ACTION claim ( name username, uint64_t asset_id ) {
        require_auth(username);

        auto itr_assets = _assetid.require_find(asset_id,"Error with table find");
        check (username == itr_assets->username,"Error with auth");
      
        uint64_t current_time = current_time_point().sec_since_epoch();

        auto itr_config = _config.require_find(itr_assets->stake[0].templates,"Error with table find");
      
        check ( current_time < itr_assets->stake[0].locktime,"Timelock error");

        mint(itr_assets->stake[0].collection, itr_assets->stake[0].schema, itr_assets->stake[0].templates, username);
        vector<uint64_t> return_;
      return_.push_back(asset_id);
        action(
          permission_level{name(get_self()), name("active")},
          "atomicassets"_n,
          "transfer"_n,
          make_tuple(get_self(), username,return_, string("Claim tokens")))
        .send();
        _assetid.erase(itr_assets);
      
    }

    ACTION stakeasset(name username, uint64_t asset_id, uint32_t templates) {
      require_auth(username);

      auto itr_stake = _stake.require_find(username.value,"Error with table find");
      bool checkasset = false;
      for (int i = 0; i < itr_stake->assets.size();i++) {
        if (asset_id == itr_stake->assets[i]){
          checkasset = true;
          break;
        }
      }
      check(checkasset, "Invalid asset");

      auto itr_config = _config.require_find(templates,"Error with table find");

      uint64_t current_time = current_time_point().sec_since_epoch();
      vector <stake_nfts> _nft_config = {};
      
      _nft_config.push_back({
        .locktime = (current_time + itr_config->config[0].locktime),
        .starttime = current_time,
        .collection = itr_config->config[0].collection,
        .schema = itr_config->config[0].schema,
        .templates = itr_config->config[0].templates
      });
      _assetid.emplace(username, [&](auto& row) {
          row.asset_id = asset_id;
          row.username = username;
        
      });
    }
    ACTION addcofnig( uint64_t locktime, uint32_t template_, name collection,name schema,uint32_t templates ) {
        require_auth(get_self());

        auto itr_config = _config.find(template_);

        if ( itr_config == _config.end() ) {
            _config.emplace(get_self(), [&](auto& row) {
                row.templates = template_;
                row.config.push_back({
                    .locktime = locktime,
                    .collection = collection,
                    .schema = schema,
                    .templates = templates
                });
            });
        } else {
            _config.modify(itr_config, get_self(), [&](auto& row) {
                row.config.push_back({
                    .locktime = locktime,
                    .collection = collection,
                    .schema = schema,
                    .templates = templates,
                });
            });
        }
    }
 private:

    struct stake_nfts
    {   
        uint64_t locktime;
        uint64_t starttime;
        
        name collection;
        name schema;
        uint32_t templates;
    };

    TABLE assetid {
        uint64_t asset_id;
        name username;
        vector<stake_nfts> stake;

        auto primary_key() const { return asset_id; }
      };

    typedef multi_index<name("assets"), assetid> assetids;
    assetids _assetid;


    struct nft_config
    {   
        uint64_t locktime;
        
        name collection;
        name schema;
        uint32_t templates;
    };


    TABLE config { 
        uint32_t templates;
        vector<nft_config> config;
        
        auto primary_key() const { return templates; }
    };

    typedef multi_index<name("config"), config> configs;
    configs _config;

  TABLE stake { 
        name username;
        vector<uint64_t> assets;
        
        auto primary_key() const { return username.value; }
    };

    typedef multi_index<name("stake"), stake> stakes;
    stakes _stake;

    void mint(name collection, name shemas, uint32_t templates,name owner) {
        vector<name> data;
        action{
            permission_level{get_self(), "active"_n},
            "atomicassets"_n,
            "mintasset"_n,
            std::make_tuple(get_self(),collection, shemas, templates, owner, data, data, data)
        }.send();
    }
};





