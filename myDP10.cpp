// 実行方法 DP.exe database length -k keyword1 keyword2 ... -a ambience1 ambience2 ...

#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <cstdio>
// #include <omp.h>
#include <cstdlib>
#include <ctime>
using namespace std;

#define MAX_DICTIONARY  3000                   // 最大の辞書のサイズ
#define MAX_KEYWORD     7                      // 最大入力可能なキーワード数+2
#define MAX_AMBIENCE    10                     // 共起する語の最大数
#define MIN_FRAGMENT    2                      // DPfragmentの最小の単語数
#define FRAGMENT_RANGE  10                     // DPfragmentの単語数の幅
#define MAX_FRAGMENT    MIN_FRAGMENT+FRAGMENT_RANGE // DPfragmentの最大の単語数
#define MAX_LYRICS      30                     // 出力の最大の単語数
#define LYRICS_RANGE    2                      // 出力歌詞の文字数のずれの許容幅
#define MAX_CANDIDATE   20                     // 出力候補のバッファ
#define BEAM_WIDTH      100
#define AMBIENCE_WEIGHT 10.0

int dictionary_length;                         // from .DATファイル
float bigram[MAX_DICTIONARY][MAX_DICTIONARY];  // from .DATファイル
int collocate[MAX_DICTIONARY][MAX_DICTIONARY]; // from .DATファイル
int word_length[MAX_DICTIONARY];               // from .DATファイル → from _D.TXTファイル
string dictionary[MAX_DICTIONARY][4];          // from _D.TXTファイル

class DPfragment{ // キーワード間のDPをしたときの各断片に対するクラス
public:
  int body[MAX_LYRICS]; // 断片の単語のインデックスの配列
  int head;             // 最初の文字のインデックス
  int tail;             // 最後の文字のインデックス
  int num_word;         // 断片内の単語数
  int num_mora;         // 断片全体の文字数（但し、最後の単語は除く）
  float prob;           // 断片が出現する確率（対数）
  bool valid;           // 正しい断片かどうかの判定
  
  DPfragment(){
    head = tail = -1; num_word = num_mora = 0;
    prob = logf(0.0); valid = false;
  }
  
  int count_mora(){
    num_mora = 0;
    for(int i=0; i<(num_word-1); i++)
      num_mora += word_length[body[i]];
    return num_mora;
  }

  void add(DPfragment dpf2){
    int tmp = num_word + dpf2.num_word -1;
    if(tail==dpf2.head && tmp<=MAX_LYRICS){
      for(int i=num_word; i<tmp; i++)
        body[i] = dpf2.body[i-num_word+1];
      num_word = tmp;
      num_mora += dpf2.num_mora;
      prob += dpf2.prob;
      tail = dpf2.tail;
    } else {
      valid = false;
    }
  }

};

class DPnode{
public:
  int index;
  int trace_back;
  float prob;
  
  DPnode(){ index = -1; trace_back = -1; prob = logf(0.0); }
  void set(int i,int t,float p){ index = i; trace_back = t; prob = p; }
};

void dpmatching_bs(DPnode dpnode[BEAM_WIDTH][MAX_FRAGMENT],
                   float unigram[MAX_DICTIONARY], int head, int tail){
  int k, buff, argmax;
  float max_prob, prob;
  
  dpnode[0][0].set(head, -1, logf(1.0));
  for(k=1; k<BEAM_WIDTH; k++) dpnode[k][0].set(-1, -1, logf(0.0));

  for(int t=1; t<MAX_FRAGMENT; t++){
    buff = 1;
    for(int j=0; j<dictionary_length; j++){
      max_prob = logf(0.0);
      argmax = 0;
      for(k=0; k<BEAM_WIDTH; k++){
        if(dpnode[k][t-1].index >= 0){
          if((prob = bigram[dpnode[k][t-1].index][j] + dpnode[k][t-1].prob) > max_prob){
            max_prob = prob;
            argmax = k;
          }
        }
      }
      max_prob += unigram[j];
      if(j == tail){
        dpnode[0][t].set(tail, argmax, max_prob);
      } else {
        for(k=buff; k>1; k--){
          if(k < BEAM_WIDTH){
            if(max_prob > dpnode[k-1][t].prob){
              dpnode[k][t] = dpnode[k-1][t];
            } else {
              dpnode[k][t].set(j, argmax, max_prob);
              break;
            }
          } else if(max_prob <= dpnode[k-1][t].prob){
            break;
          }
        }
        if(k == 1) dpnode[1][t].set(j, argmax, max_prob);
        if(buff < BEAM_WIDTH) buff++;
      }
    }
  }
};




int main(int argc , char*argv[]){
  int i, j;
  string str;
  // from オプション
  string dic_filename, dat_filename;
  int lyrics_length, num_keyword, num_ambience;

  // オプション読み込み
  dic_filename = (string)argv[1] + "_d.txt";
  dat_filename = (string)argv[1] + ".dat";
  lyrics_length = atoi(argv[2]);
  if((string)argv[3] != "-k"){ cout << "error: -k not found" << endl; exit(EXIT_FAILURE); }
  for(i=0; (string)argv[i] != "-a"; i++);
  num_keyword = i-4;
  num_ambience = argc-i-1;

  // .DATファイル読み込み
  FILE *fp = fopen( dat_filename.c_str(), "rb");
  if(fp==NULL){ cout << "error: bigram file not found" << endl; exit(EXIT_FAILURE); }
  fread(&dictionary_length, sizeof(int), 1, fp);
  for(i=0; i<dictionary_length; i++)
    fread(bigram[i], sizeof(float), dictionary_length, fp);
  for(i=0; i<dictionary_length; i++)
    fread(collocate[i], sizeof(int), dictionary_length, fp);
//  fread(word_length, sizeof(int), dictionary_length, fp);
  fclose(fp);

  // _D.TXTファイル読み込み TODO:最後の空行を読み飛ばすように変更
  // dictionary[j][0]活用形 [1]読み長さ(string) [2]基本形 [3]登場回数(string)
  ifstream dic_file( dic_filename.c_str() );
  if(!dic_file.is_open()){ cout << "error: dictionary file not found" << endl; exit(EXIT_FAILURE); }
  j = 0;
  while(!dic_file.eof()){
    getline(dic_file, str);
    str = str.substr(str.find_first_of("-") + 1); // indexは捨てる
    dictionary[j][0] = str.substr(0, str.find_first_of("-"));
    str = str.substr(str.find_first_of("-") + 1);
    dictionary[j][1] = str.substr(0, str.find_first_of("-"));
    word_length[j] = atoi(dictionary[j][1].c_str());
    str = str.substr(str.find_first_of("-") + 1);
    dictionary[j][2] = str.substr(0, str.find_first_of("-"));
    str = str.substr(str.find_first_of("-") + 1);
    dictionary[j][3] = str.substr(0, str.find_first_of("-"));
    j++;
  }
  dic_file.close();
  
  // キーワード、アンビエンス → インデックス
  // TODO:基本形で指定されたキーワードを乱数で他の活用形に散らす
  // srand((unsigned)time(NULL)); counter = rand() % counter + 1;
  int keyword_index[MAX_KEYWORD], ambience_index[MAX_AMBIENCE];
  bool keyword_known[MAX_KEYWORD], ambience_known[MAX_AMBIENCE];
  for(i=0; i<num_keyword+2; i++) keyword_known[i] = false;
  for(i=0; i<num_ambience+1; i++) ambience_known[i] = false;
  keyword_index[0] = 0;             // BOS
  keyword_index[num_keyword+1] = 1; // EOS
  keyword_known[0] = true;
  keyword_known[num_keyword+1] = true;  
/*
  for(i=1; i<=num_keyword; i++){
    for(j=0; j<dictionary_length; j++){
      if(dictionary[j][2] == argv[i+3]){
        keyword_index[i] = j;
        keyword_known[i] = true;
      }
    }
  }
  for(i=0; i<num_ambience; i++){
    for(j=0; j<dictionary_length; j++){
      if(dictionary[j][2] == argv[i+num_keyword+5]){
        ambience_index[i] = j;
	ambience_known[i] = true;
      }
    }
  }
*/
  for(j=0; j<dictionary_length; j++) if(dictionary[j][2] == "夢") keyword_index[1] = j;
  for(j=0; j<dictionary_length; j++) if(dictionary[j][2] == "長い") keyword_index[2] = j;
  for(j=0; j<dictionary_length; j++) if(dictionary[j][2] == "愛") ambience_index[0] = j;
  
  // 共起させる単語に対しては重みを100、それ以外の単語に関しては重みを1にする  
  float unigram[MAX_DICTIONARY];
  for(i=0; i<dictionary_length; i++){
    unigram[i] = 1.0;
    for(j=0; j<num_ambience; j++){
      if(i < ambience_index[j]){
        if(collocate[ambience_index[j]][i] == 1.0){
          unigram[i] = AMBIENCE_WEIGHT;
        }
      } else if(i == ambience_index[j]){
        unigram[i] = AMBIENCE_WEIGHT;
      } else {
        if(collocate[i][ambience_index[j]] == 1.0){
          unigram[i] = AMBIENCE_WEIGHT;
        }
      }
    }
  }

  // bigram[][]とunigram[]の対数化
  for(i=0; i<dictionary_length; i++)
    for(j=0; j<dictionary_length; j++)
      bigram[i][j] = logf(bigram[i][j]);
  for(i=0; i<dictionary_length; i++)
    unigram[i] = logf(unigram[i]);

  // 2段DP
  int buff_size, loop[MAX_KEYWORD];
  DPfragment fragment[MAX_KEYWORD][FRAGMENT_RANGE];
  DPfragment lyrics[MAX_CANDIDATE], tmpdp;
  DPnode dpnode_bs[BEAM_WIDTH][MAX_FRAGMENT];
  float normalize[MAX_DICTIONARY];
  for(i=0; i<dictionary_length; i++) normalize[i] = 0.0;

  for(i=0; i<=num_keyword; i++){ // キーワード間のDPfragmentの計算
    dpmatching_bs(dpnode_bs, unigram, keyword_index[i], keyword_index[i+1]);
    for(j=0; j<FRAGMENT_RANGE; j++){
      fragment[i][j].num_word = MIN_FRAGMENT+j;
      fragment[i][j].prob = dpnode_bs[0][MIN_FRAGMENT+j-1].prob;
      fragment[i][j].head = keyword_index[i];
      fragment[i][j].tail = keyword_index[i+1];
// --- trace_back_bs(fragment[i][j].route, dpnode_bs, fragment[i][j].tail, fragment[i][j]. num_word);
      fragment[i][j].body[fragment[i][j].num_word-1] = fragment[i][j].tail;
      int tmp = 0;
      for(int k=fragment[i][j].num_word-2; k>=0; k--){
        tmp = dpnode_bs[tmp][k+1].trace_back;
        fragment[i][j].body[k]=dpnode_bs[tmp][k].index;
      }
// ---
      fragment[i][j].count_mora();
      fragment[i][j].valid = true;
    }
  }

  for(i=0; i<=num_keyword; i++) loop[i] = 0;
  buff_size = 0;

  // 計算したDPfragmentの可能なすべてのつなぎ方に対して文字数を計算し、
  // 条件に合う文字数をもつ文を確率が大きいほうからMAX_CANDIDATEだけ求める
  while(true){
    tmpdp = fragment[0][loop[0]];
//    tmpdp = DPfragment(fragment[0][loop[0]]);
    for(i=1; i<=num_keyword; i++)
      tmpdp.add(fragment[i][loop[i]]);
    if( (tmpdp.num_mora <= (lyrics_length+LYRICS_RANGE)) &&
        (tmpdp.num_mora >= (lyrics_length-LYRICS_RANGE)) &&
        (tmpdp.prob > logf(0.0)) ){
      for(i=buff_size; i>0; i--){
        if(i < MAX_CANDIDATE){
          if(lyrics[i-1].prob < tmpdp.prob){
            lyrics[i] = lyrics[i-1];
          } else {
            lyrics[i] = tmpdp;
            break;
          }
        } else if(lyrics[i-1].prob >= tmpdp.prob){
          break;
        }
      }
      if(i==0) lyrics[0] = tmpdp;
      if(buff_size < MAX_CANDIDATE) buff_size++;
    }
    loop[0]++;
    for(i=0; i<num_keyword; i++){
      if(loop[i] >= FRAGMENT_RANGE){
        loop[i] = 0;
        loop[i+1]++;
      }
    }
    if(loop[num_keyword] >= FRAGMENT_RANGE)  break;
  }

  if(buff_size == 0) cout << "error: buffer empty" << endl;
  // ↓buff_size==0のとき挙動がおかしくなる
  for(i=0; i<5; i++){
    for(j=0; j<lyrics[i].num_word; j++){
      cout << dictionary[lyrics[i].body[j]][0];
      // cout << lyrics[i].prob << endl;
    }
    cout << endl;
  }

  return 0;
}
