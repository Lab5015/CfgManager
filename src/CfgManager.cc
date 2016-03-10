#include "interface/CfgManager.h"
#include "interface/CfgManagerT.h"

//**********utils*************************************************************************

//----------Check if the key is in cfg----------------------------------------------------
bool CfgManager::OptExist(string key, int opt)
{
    for(auto& iopt : opts_)
        if(iopt.first == "opts."+key && iopt.second.size()>opt)
            return true;

    return false;
}

//----------Help method, parse single line------------------------------------------------
void CfgManager::ParseSingleLine(const string& line, vector<string>& tokens)
{
    //---parsing utils
    size_t prev=0, pos;
    string delimiter=" ";

    //---line loop
    while((pos = line.find_first_of(delimiter, prev)) != string::npos)
    {
        if(pos > prev)
            tokens.push_back(line.substr(prev, pos-prev));
        prev = pos+1;
        if(line[prev] == '\'')
        {
            delimiter = "\'";
            ++prev;
        }
        else
            delimiter = " ";
    }
    if(prev < line.length())
        tokens.push_back(line.substr(prev, string::npos));

    return;
}

//----------Parse configuration file and setup the configuration--------------------------
void CfgManager::ParseConfigFile(const char* file)
{
    cout << "> CfgManager --- INFO: parsing " << file << endl;
    //---read config file
    ifstream cfgFile(file, ios::in);
    string buffer;
    string current_block="opts";
    while(getline(cfgFile, buffer))
    {
        if(buffer.size() == 0 || buffer.at(0) == '#')
            continue;        

        //---parse the current line
        vector<string> tokens;
        ParseSingleLine(buffer, tokens);
        while(tokens.back() == "\\")
        {
            tokens.pop_back();
            getline(cfgFile, buffer);
            ParseSingleLine(buffer, tokens);
        }

        //---store the option inside the current configuration
        HandleOption(current_block, tokens);
    }
    cfgFile.close();

    //---set automatic info
    char hostname[100];
    gethostname(hostname, 100);
    username_ = string(getpwuid(getuid())->pw_name)+"@"+hostname;
    time_t rawtime;
    time(&rawtime);
    struct tm* t = localtime(&rawtime);
    timestamp_ = to_string(t->tm_mday)+"/"+to_string(t->tm_mon)+"/"+to_string(t->tm_year+1900)+"  "+
        to_string(t->tm_hour)+":"+to_string(t->tm_min)+":"+to_string(t->tm_sec);

    return;
}

//----------Parse configuration string and setup the configuration------------------------
void CfgManager::ParseConfigString(const string config)
{
    string current_block="opts";
    //---parse the current string
    vector<string> tokens;
    ParseSingleLine(config, tokens);
    HandleOption(current_block, tokens);

    return;
}

//----------Handle single option: key and parameters--------------------------------------
//---private methode called by ParseConfig public methods
void CfgManager::HandleOption(string& current_block, vector<string>& tokens)
{
    //---new block
    if(tokens.at(0).at(0) == '<')
    {
        //---close previous block
        if(tokens.at(0).at(1) == '/')
        {
            tokens.at(0).erase(tokens.at(0).begin(), tokens.at(0).begin()+2);
            tokens.at(0).erase(--tokens.at(0).end());
            int last_dot = current_block.find_last_of(".");
            if(tokens.at(0) == current_block.substr(last_dot+1))
                current_block.erase(last_dot);
            else
                cout << "> CfgManager --- ERROR: wrong closing block // " << tokens.at(0) << endl;
        }
        //---open new block
        else
        {
            tokens.at(0).erase(tokens.at(0).begin());
            tokens.at(0).erase(--tokens.at(0).end());
            current_block += "."+tokens.at(0);
        }
    }
    //---copy block
    else if(tokens.at(0) == "copyBlocks")
    {
        tokens.erase(tokens.begin());
        for(auto& block_to_copy: tokens)
            CopyBlock(current_block, block_to_copy);
    }
    //---import cfg
    else if(tokens.at(0) == "importCfg")
    {
        tokens.erase(tokens.begin());
        for(auto& cfgFile: tokens)
            ParseConfigFile(cfgFile.c_str());
    }
    //---option line
    else
    {
        string key=tokens.at(0);
        tokens.erase(tokens.begin());
            
        //---update key
        if(key.substr(key.size()-2) == "+=")
        {
            key = key.substr(0, key.size()-2);
            for(auto& token : tokens)
            {
                if(OptExist(token))
                {
                    auto extend_opt = GetOpt<vector<string> >(token);
                    opts_[current_block+"."+key].insert(opts_[current_block+"."+key].end(),
                                                        extend_opt.begin(),
                                                        extend_opt.end());
                }
                else
                    opts_[current_block+"."+key].push_back(token);
            }
        }
        //---copy key
        else if(key.substr(key.size()-1) == "=" && tokens.size() > 0)
        {
            key = key.substr(0, key.size()-1);
            if(OptExist(tokens.at(0)))
                opts_[current_block+"."+key] = GetOpt<vector<string> >(tokens.at(0));
            else
                cout << "> CfgManager --- WARNING: undefined option // " << tokens.at(0) << endl;
        }
        //---new key
        else 
            opts_[current_block+"."+key] = tokens;
    }

    return;
}

//----------Copy entire block-------------------------------------------------------------
//---already defined option are overridden
void CfgManager::CopyBlock(string& current_block, string& block_to_copy)
{
    bool found_any=false;
    //---copy block entries
    for(auto& opt : opts_)
    {
        string key = opt.first;
        vector<string> opts = opt.second;
        if(key.find(block_to_copy) != string::npos)
        {
            string new_key = key;
            new_key.replace(key.find(block_to_copy), block_to_copy.size(), current_block.substr(5));
            opts_[new_key] = opts;
        }
    }
    //---block not found
    if(!found_any)
        cout << "> CfgManager --- WARNING: requested block (" << block_to_copy << ") is not defined" << endl;

    return;
}

//----------Print formatted version of the cfg--------------------------------------------
//---option is the key to be print: default value meas "all keys"
void CfgManager::Print(Option_t* option) const
{
    string argkey = option;
    //---banner
    string banner = "configuration was created by "+username_+" on "+timestamp_;
    for(int i=0; i<banner.size(); ++i)
        cout << "=";
    cout << endl;
    cout << banner << endl;
    for(int i=0; i<banner.size(); ++i)
        cout << "=";
    cout << endl;
    
    //---options
    string prev_block="";
    for(auto& key : opts_)
    {
        string current_block = key.first.substr(5, key.first.find_last_of(".")-5);
        if(argkey == "" || key.first.find(argkey) != string::npos)
        {
            if(current_block != prev_block)
            {
                if(prev_block != "")
                    cout << "+----------" << endl;
                cout << current_block << ":" << endl;
                prev_block = current_block;
            }
            cout << "|----->" << key.first.substr(key.first.find_last_of(".")+1) << ": ";
            for(auto& opt : key.second)
                cout << opt << ", " ;
            cout << endl;
        }
    }
    cout << "+----------" << endl;

    return;
}

//----------Internal error check----------------------------------------------------------
void CfgManager::Errors(string key, int opt)
{
    if(opts_.count(key) == 0)
    {
        cout << "> CfgManager --- ERROR: key '"<< key.substr(5, key.size()) << "' not found" << endl;
        exit(-1);
    }
    if(opt >= opts_[key].size())
    {
        cout << "> CfgManager --- ERROR: option '"<< key.substr(5, key.size()) << "' as less then "
             << opt << "values (" << opts_[key].size() << ")" << endl;
        exit(-1);
    }
    return;
}

//**********operators*********************************************************************

ostream& operator<<(ostream& out, const CfgManager& obj)
{
    //---banner
    out << "configuration: " << endl;
    //---options
    for(auto& key : obj.opts_)
    {
        out << key.first.substr(5) << ":" << endl;
        for(auto& opt : key.second)
            out << "\t" << opt << endl;
    }
    return out;
}
