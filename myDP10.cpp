// ���s���@ DP.exe database length -k keyword1 keyword2 ... -a ambience1 ambience2 ...

#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <cstdio>
// #include <omp.h>
#include <cstdlib>
#include <ctime>
using namespace std;

#define MAX_DICTIONARY  3000                   // �ő�̎����̃T�C�Y
#define MAX_KEYWORD     7                      // �ő���͉\�ȃL�[���[�h��+2
#define MAX_AMBIENCE    10                     // ���N�����̍ő吔
#define MIN_FRAGMENT    2                      // DPfragment�̍ŏ��̒P�ꐔ
#define FRAGMENT_RANGE  10                     // DPfragment�̒P�ꐔ�̕�
#define MAX_FRAGMENT    MIN_FRAGMENT+FRAGMENT_RANGE // DPfragment�̍ő�̒P�ꐔ
#define MAX_LYRICS      30                     // �o�͂̍ő�̒P�ꐔ
#define LYRICS_RANGE    2                      // �o�͉̎��̕������̂���̋��e��
#define MAX_CANDIDATE   20                     // �o�͌��̃o�b�t�@
#define BEAM_WIDTH      100
#define AMBIENCE_WEIGHT 10.0

int dictionary_length;                         // from .DAT�t�@�C��
float bigram[MAX_DICTIONARY][MAX_DICTIONARY];  // from .DAT�t�@�C��
int collocate[MAX_DICTIONARY][MAX_DICTIONARY]; // from .DAT�t�@�C��
int word_length[MAX_DICTIONARY];               // from .DAT�t�@�C�� �� from _D.TXT�t�@�C��
string dictionary[MAX_DICTIONARY][4];          // from _D.TXT�t�@�C��

class DPfragment{ // �L�[���[�h�Ԃ�DP�������Ƃ��̊e�f�Ђɑ΂���N���X
public:
  int body[MAX_LYRICS]; // �f�Ђ̒P��̃C���f�b�N�X�̔z��
  int head;             // �ŏ��̕����̃C���f�b�N�X
  int tail;             // �Ō�̕����̃C���f�b�N�X
  int num_word;         // �f�Г��̒P�ꐔ
  int num_mora;         // �f�БS�̂̕������i�A���A�Ō�̒P��͏����j
  float prob;           // �f�Ђ��o������m���i�ΐ��j
  bool valid;           // �������f�Ђ��ǂ����̔���
  
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
  // from �I�v�V����
  string dic_filename, dat_filename;
  int lyrics_length, num_keyword, num_ambience;

  // �I�v�V�����ǂݍ���
  dic_filename = (string)argv[1] + "_d.txt";
  dat_filename = (string)argv[1] + ".dat";
  lyrics_length = atoi(argv[2]);
  if((string)argv[3] != "-k"){ cout << "error: -k not found" << endl; exit(EXIT_FAILURE); }
  for(i=0; (string)argv[i] != "-a"; i++);
  num_keyword = i-4;
  num_ambience = argc-i-1;

  // .DAT�t�@�C���ǂݍ���
  FILE *fp = fopen( dat_filename.c_str(), "rb");
  if(fp==NULL){ cout << "error: bigram file not found" << endl; exit(EXIT_FAILURE); }
  fread(&dictionary_length, sizeof(int), 1, fp);
  for(i=0; i<dictionary_length; i++)
    fread(bigram[i], sizeof(float), dictionary_length, fp);
  for(i=0; i<dictionary_length; i++)
    fread(collocate[i], sizeof(int), dictionary_length, fp);
//  fread(word_length, sizeof(int), dictionary_length, fp);
  fclose(fp);

  // _D.TXT�t�@�C���ǂݍ��� TODO:�Ō�̋�s��ǂݔ�΂��悤�ɕύX
  // dictionary[j][0]���p�` [1]�ǂݒ���(string) [2]��{�` [3]�o���(string)
  ifstream dic_file( dic_filename.c_str() );
  if(!dic_file.is_open()){ cout << "error: dictionary file not found" << endl; exit(EXIT_FAILURE); }
  j = 0;
  while(!dic_file.eof()){
    getline(dic_file, str);
    str = str.substr(str.find_first_of("-") + 1); // index�͎̂Ă�
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
  
  // �L�[���[�h�A�A���r�G���X �� �C���f�b�N�X
  // TODO:��{�`�Ŏw�肳�ꂽ�L�[���[�h�𗐐��ő��̊��p�`�ɎU�炷
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
  for(j=0; j<dictionary_length; j++) if(dictionary[j][2] == "��") keyword_index[1] = j;
  for(j=0; j<dictionary_length; j++) if(dictionary[j][2] == "����") keyword_index[2] = j;
  for(j=0; j<dictionary_length; j++) if(dictionary[j][2] == "��") ambience_index[0] = j;
  
  // ���N������P��ɑ΂��Ă͏d�݂�100�A����ȊO�̒P��Ɋւ��Ă͏d�݂�1�ɂ���  
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

  // bigram[][]��unigram[]�̑ΐ���
  for(i=0; i<dictionary_length; i++)
    for(j=0; j<dictionary_length; j++)
      bigram[i][j] = logf(bigram[i][j]);
  for(i=0; i<dictionary_length; i++)
    unigram[i] = logf(unigram[i]);

  // 2�iDP
  int buff_size, loop[MAX_KEYWORD];
  DPfragment fragment[MAX_KEYWORD][FRAGMENT_RANGE];
  DPfragment lyrics[MAX_CANDIDATE], tmpdp;
  DPnode dpnode_bs[BEAM_WIDTH][MAX_FRAGMENT];
  float normalize[MAX_DICTIONARY];
  for(i=0; i<dictionary_length; i++) normalize[i] = 0.0;

  for(i=0; i<=num_keyword; i++){ // �L�[���[�h�Ԃ�DPfragment�̌v�Z
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

  // �v�Z����DPfragment�̉\�Ȃ��ׂĂ̂Ȃ����ɑ΂��ĕ��������v�Z���A
  // �����ɍ������������������m�����傫���ق�����MAX_CANDIDATE�������߂�
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
  // ��buff_size==0�̂Ƃ����������������Ȃ�
  for(i=0; i<5; i++){
    for(j=0; j<lyrics[i].num_word; j++){
      cout << dictionary[lyrics[i].body[j]][0];
      // cout << lyrics[i].prob << endl;
    }
    cout << endl;
  }

  return 0;
}
