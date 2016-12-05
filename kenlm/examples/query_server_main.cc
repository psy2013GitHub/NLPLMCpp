
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

#include "lm/model.hh"
#include "util/tokenize_piece.hh"
#include "util/string_piece.hh"
#include "util/file_piece.hh"

#include "cppjieba/include/cppjieba/Jieba.hpp"

#include "Simple-Web-Server/server_http.hpp"
#include "Simple-Web-Server/client_http.hpp"

//Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//Added for the default_resource example
#include <fstream>
#include <boost/filesystem.hpp>

using namespace std;
using namespace cppjieba;
using namespace lm::ngram;
//Added for the json-example:
using namespace boost::property_tree;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;

const char* const DICT_PATH = "./cppjieba/dict/jieba.dict.utf8";
const char* const HMM_PATH = "./cppjieba/dict/hmm_model.utf8";
const char* const USER_DICT_PATH = "./cppjieba/dict/user.dict.utf8";
const char* const IDF_PATH = "./cppjieba/dict/idf.utf8";
const char* const STOP_WORD_PATH = "./cppjieba/dict/stop_words.utf8";


std::string UrlEncode(const std::string& szToEncode) {
    std::string src = szToEncode;
    char hex[] = "0123456789ABCDEF";
    string dst;
 
    for (size_t i = 0; i < src.size(); ++i)
    {
        unsigned char cc = src[i];
        if (isascii(cc))
        {
            if (cc == ' ')
            {
                dst += "%20";
            }
            else
                dst += cc;
        }
        else
        {
            unsigned char c = static_cast<unsigned char>(src[i]);
            dst += '%';
            dst += hex[c / 16];
            dst += hex[c % 16];
        }
    }
    return dst;
}
 
 
std::string UrlDecode(const std::string& szToDecode) {
    std::string result;
    int hex = 0;
    for (size_t i = 0; i < szToDecode.length(); ++i)
    {
        switch (szToDecode[i])
        {
        case '+':
            result += ' ';
            break;
        case '%':
            if (isxdigit(szToDecode[i + 1]) && isxdigit(szToDecode[i + 2]))
            {
                std::string hexStr = szToDecode.substr(i + 1, 2);
                hex = strtol(hexStr.c_str(), 0, 16);
                //字母和数字[0-9a-zA-Z]、一些特殊符号[$-_.+!*'(),] 、以及某些保留字[$&+,/:;=?@]
                //可以不经过编码直接用于URL
                if (!((hex >= 48 && hex <= 57) || //0-9
                    (hex >=97 && hex <= 122) ||   //a-z
                    (hex >=65 && hex <= 90) ||    //A-Z
                    //一些特殊符号及保留字[$-_.+!*'(),]  [$&+,/:;=?@]
                    hex == 0x21 || hex == 0x24 || hex == 0x26 || hex == 0x27 || hex == 0x28 || hex == 0x29
                    || hex == 0x2a || hex == 0x2b|| hex == 0x2c || hex == 0x2d || hex == 0x2e || hex == 0x2f
                    || hex == 0x3A || hex == 0x3B|| hex == 0x3D || hex == 0x3f || hex == 0x40 || hex == 0x5f
                    ))
                {
                    result += char(hex);
                    i += 2;
                }
                else result += '%';
            }else {
                result += '%';
            }
            break;
        default:
            result += szToDecode[i];
            break;
        }
    }
    return result;
}

int main() {
    /* init ngram model & jieba */
    char *model_path = "/your binary model path"; // fixme, set as your path
    
    //std::cout << "load model ..." << std::endl;
    Model model(model_path);
    State state, out_state;
    lm::FullScoreReturn ret;
    //std::cout << "loading vocabulary ... " << std::endl;
    const Vocabulary &vocab = model.GetVocabulary();
 

    /* jieba分词 */
    cout << "jieba init..." << endl;
    cppjieba::Jieba jieba(DICT_PATH,
        HMM_PATH,
        USER_DICT_PATH
    );

    //HTTP-server at port 8080 using 4 threads
    int port = 8002;
    int n_threads = 4;
    HttpServer server(port, n_threads);
    
    //Add resources using path-regex and method-string, and an anonymous function
    //POST-example for the path /string, responds the posted string
    server.resource["^/string$"]["POST"]=[](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        //Retrieve string:
        auto content=request->content.string();
        //request->content.string() is a convenience function for:
        //stringstream ss;
        //ss << request->content.rdbuf();
        //string content=ss.str();
        
        response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };
    
    server.resource["^/json$"]["POST"]=[&](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        try {
            uint64_t oov = 0;
            float total = 0.0f;
            float corpus_total = 0.0;
            double corpus_total_oov_only = 0.0;
            uint64_t corpus_oov = 0;
            uint64_t corpus_tokens = 0;           

            ptree pt;
            vector<string> params;
            read_json(request->content, pt);

            string service=pt.get<string>("task_name");
            cout << "service:" << service << endl;
            ptree params_pt=pt.get_child("params");
            for(const ptree::value_type &v : params_pt) {  
                params.push_back(v.second.get_value<string>());  
            }
            string line = UrlDecode(params[0]);
            int order=std::atoi(params[1].c_str());
            cout << "input: " << line << " " << order << endl;

            vector<pair<string, string> > tagres;
            vector<pair<string, string> >::iterator iter;
            vector<string> words;
            string tokenized_line;

            cout << "tag: " << endl;
            jieba.Tag(line, tagres);
            
            for (iter = tagres.begin(); iter!=tagres.end(); ++iter) {
                cout << iter->first << " " << iter->second << endl;
                if (iter->second.compare("m")==0) {
                   words.push_back("<\\/num>");
                } else {
                   words.push_back(iter->first); 
                }
            }   
            tokenized_line = limonp::Join(words.begin(), words.end(), " ");
            cout << "tokenized_line: " << tokenized_line << endl;
             
            cout << "look_up..." << endl;
            int sentence_context = 1; 
           
            vector<lm::WordIndex> unigram_w_idx_vec; 
            for (int j=words.size()-1; j>-1; --j) { // 反序
                lm::WordIndex w_idx = model.GetVocabulary().Index(words[j]);
                unigram_w_idx_vec.push_back(w_idx);
            }

            string ngram_str;
            float ngram_cond_score = 0.0f, ngram_full_score = 0.0f;
            vector<vector<pair<string, pair<float, float> > > > ngram_score_vec;	 	
            for (int i=0; i<order; ++i) { // i+1 是 order
               vector<pair<string, pair<float, float> > > tmp_vec;
               for (int j=0; i+j < words.size(); ++j) {
                  lm::WordIndex w_idx = model.GetVocabulary().Index(words[j]);
                  ngram_str= words[j];
                  ngram_cond_score = 0.0f, ngram_full_score = 0.0f;
                  state = model.NullContextState(); // 默认状态
                  if (i > 0) { // 当 两个以上词时 才join
                     lm::WordIndex* context_rbegin = &(unigram_w_idx_vec[words.size()-1-j-i+1]);  // context_rbegin 
                     lm::WordIndex* context_rend = &(unigram_w_idx_vec[words.size()-1-j+1]); // context_rend
                     ngram_str = limonp::Join(words.begin()+j, words.begin()+j+i+1, " ");
                     w_idx = model.GetVocabulary().Index(words[j+i]);
                     ret = model.FullScoreForgotState(context_rbegin, context_rend, w_idx, out_state); 
                     //cout << "ngram: " << context_rend - context_rbegin << " " << ngram_str << " " << ret.ngram_length - 0 << " " << ret.prob << endl;
                     if (ret.ngram_length - 0 == i + 1) { // 只有存在时计算
                        ngram_cond_score = ret.prob;
                        for (int k=j; k<j+i+1; ++k) {
                            ret = model.FullScore(state, unigram_w_idx_vec[words.size()-1-k], out_state);
                            ngram_full_score += ret.prob;
                            state = out_state;
                        }
                        //ret = model.FullScore(state, model.GetVocabulary().EndSentence(), out_state);
                        //score += ret.prob;
                     }
                  } else if (w_idx != model.GetVocabulary().NotFound()) { // 字典中存在得unigram, 对于ngram ngram_cond_score = ngram_full_score
                     ngram_cond_score = model.BaseScore(&state, w_idx, &out_state);
                     ngram_full_score = ngram_cond_score;
                  }
                  tmp_vec.push_back(std::make_pair(ngram_str, std::make_pair(ngram_cond_score, ngram_full_score)));
               } 
               ngram_score_vec.push_back(tmp_vec);
            }

           /* 输出json */
           ptree ngram_score_outer;
           int curr_order = 0;
           for (; curr_order < ngram_score_vec.size(); ++curr_order) {
              ptree ngram_score_inner;
              for (int j=0; j < ngram_score_vec[curr_order].size(); ++j) {
                 ptree ngram;
                 ptree score1;
                 ptree score2;
                 ptree score_pair;
                 ptree ngram_score_pair;
                 ngram.put("", ngram_score_vec[curr_order][j].first); 
                 score1.put("", std::to_string(ngram_score_vec[curr_order][j].second.first));
                 score_pair.push_back(std::make_pair("", score1));
                 if (curr_order > 0) {
                   score2.put("", std::to_string(ngram_score_vec[curr_order][j].second.second));
                   score_pair.push_back(std::make_pair("", score2));
                 }
                 ngram_score_pair.push_back(std::make_pair("", ngram));
                 ngram_score_pair.push_back(std::make_pair("", score_pair));
                 ngram_score_inner.push_back(std::make_pair("", ngram_score_pair));
              } 
              ngram_score_outer.push_back(std::make_pair(std::to_string(curr_order+1), ngram_score_inner));
           }
           for (; curr_order+1 < order; ++curr_order) {
              ptree ngram;
              ngram.put("", "-11111");
              ptree score;
              score.put("", "-11111");
              ptree useless;
              useless.push_back(std::make_pair("", ngram));
              useless.push_back(std::make_pair("", score)); 
              //ngram_score_outer.push_back(std::to_string(curr_order+2), " ");
              ngram_score_outer.push_back(std::make_pair(std::to_string(curr_order+2), useless));
           }
           ostringstream os;
           write_json(os, ngram_score_outer);
           string json_string = os.str();
           cout << json_string << endl;
           json_string = UrlEncode(json_string);
           response << "HTTP/1.1 200 OK\r\nContent-Length: " << json_string.length() << "\r\n\r\n" << json_string;
        }
        catch(exception& e) {
           response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
        }
    };
    
    //GET-example for the path /info
    //Responds with request-information
    server.resource["^/info$"]["GET"]=[](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        stringstream content_stream;
        content_stream << "<h1>Request from " << request->remote_endpoint_address << " (" << request->remote_endpoint_port << ")</h1>";
        content_stream << request->method << " " << request->path << " HTTP/" << request->http_version << "<br>";
        for(auto& header: request->header) {
            content_stream << header.first << ": " << header.second << "<br>";
        }
        
        //find length of content_stream (length received using content_stream.tellp())
        content_stream.seekp(0, ios::end);
        
        response <<  "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    };
    
    //GET-example for the path /match/[number], responds with the matched string in path (number)
    //For instance a request GET /match/123 will receive: 123
    server.resource["^/match/([0-9]+)$"]["GET"]=[](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        string number=request->path_match[1];
        response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
    };
    
    //Default GET-example. If no other matches, this anonymous function will be called. 
    //Will respond with content in the web/-directory, and its subdirectories.
    //Default file: index.html
    //Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
    server.default_resource["GET"]=[](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        boost::filesystem::path web_root_path("web");
        if(!boost::filesystem::exists(web_root_path))
            cerr << "Could not find web root." << endl;
        else {
            auto path=web_root_path;
            path+=request->path;
            if(boost::filesystem::exists(path)) {
                if(boost::filesystem::canonical(web_root_path)<=boost::filesystem::canonical(path)) {
                    if(boost::filesystem::is_directory(path))
                        path+="/index.html";
                    if(boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)) {
                        ifstream ifs;
                        ifs.open(path.string(), ifstream::in | ios::binary);
                        
                        if(ifs) {
                            ifs.seekg(0, ios::end);
                            size_t length=ifs.tellg();
                            
                            ifs.seekg(0, ios::beg);
                            
                            response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n";
                            
                            //read and send 128 KB at a time
                            size_t buffer_size=131072;
                            vector<char> buffer;
                            buffer.reserve(buffer_size);
                            size_t read_length;
                            try {
                                while((read_length=ifs.read(&buffer[0], buffer_size).gcount())>0) {
                                    response.write(&buffer[0], read_length);
                                    response.flush();
                                }
                            }
                            catch(const exception &e) {
                                cerr << "Connection interrupted, closing file" << endl;
                            }

                            ifs.close();
                            return;
                        }
                    }
                }
            }
        }
        string content="Could not open path "+request->path;
        response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };
    
    thread server_thread([&server](){
        //Start server
        server.start();
    });
    
    //Wait for server to start so that the client can connect
    this_thread::sleep_for(chrono::seconds(1));
    
    //Client examples
    HttpClient client("localhost:8080");
    auto r1=client.request("GET", "/match/123");
    cout << r1->content.rdbuf() << endl;

    /*
      {
       'task_name': 'ngram',
       'params': ['', 3]
      }
    */
    ptree pt_root;
    ptree params;
    ptree service; 
    ptree text;
    ptree order;

    pt_root.put("task_name", "ngram");
    //pt_root.add_child(service);
    text.put("", "不在尼日利亚");
    order.put("", 3);
    params.push_back(std::make_pair("", text));
    params.push_back(std::make_pair("", order));
    pt_root.add_child("params", params);

    ostringstream os;
    write_json(os, pt_root);
    //string json_string = os.str();
    string json_string = "{\"task_name\":\"ngram\",\"params\":[\"%E8%80%81%E5%90%8C%E5%AD%A6\",\"2\"]}";
    //string json_string = "%E8%80%81%E5%90%8C%E5%AD%A6%E8%80%81%E5%90%8C%E5%AD%A6%E8%80%81%E5%90%8C%E5%AD%A6%E8%80%81%E5%90%8C%E5%AD%A6";
    cout << json_string << endl;
    //string json_string = "%E8%80%81%E5%90%8C%E5%AD%A6%E8%80%81%E5%90%8C%E5%AD%A6%E8%80%81%E5%90%8C%E5%AD%A6%E8%80%81%E5%90%8C%E5%AD%A6";
    cout << json_string << endl;
    auto r2=client.request("POST", "/string", json_string);
    cout << r2->content.rdbuf() << endl;
    auto r3=client.request("POST", "/json", json_string);
    cout << r3->content.rdbuf() << endl;
        
    server_thread.join();
    
    return 0;
}
