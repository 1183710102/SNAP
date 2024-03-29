#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <set>
#include <cstdio>
#include <algorithm>
#include <math.h>


#define n_real 4938920//27
using namespace std;


char whole_gene[n_real];
map<string,set<int>> hashtable;
int k=10;//k-mer
map<pair<char,char>,int> score_matrix;//gap用'-'表示

void read_file(string fileName){
    ifstream f;
    string tmp;
    f.open(fileName,ios::in);
    if(f.fail()){
        cout << "can not open the file." << endl;
        exit(1);
    }
    if(f.good())
        getline(f,tmp);
    int z=0;
    do{
        if(f.good()){
            getline(f,tmp);
            for(string::iterator it=tmp.begin();it<tmp.end();it++){
                whole_gene[z]=(*it);
                z++;
            }
        }
    }while(!f.eof());
    f.close();
}

//为给定基因组构建哈希表
void construct_hashtable(){
    int last_mer = n_real-k;
    for(int i=0;i<=last_mer;i++){
        string tmp(whole_gene+i,k);
        hashtable[tmp].insert(i);
    }
}

//在基因组中搜索特定read出现的位置 exact match
set<int> search_gene(string read){
    int len = read.size();
    //cout << "len" << len << endl;
    set<int> s;
    string tmp(read.begin(),read.begin()+k);
    //cout << "tmp " << tmp << endl;
    set<int> location = hashtable[tmp];
    for(int i=1;i<=len-k && !location.empty();i++){
        tmp.assign(read,i,k);
        s = hashtable[tmp];
        for(set<int>::iterator iter=location.begin();iter!=location.end();iter++){
            //cout << *iter << " ";
            if(s.count((*iter)+i)==0){//如果后面一个k-mer没有连接，则这个不是精确匹配
                location.erase(iter);
            }
        }
        //cout << endl;
    }
//    if(location.empty())
//        cout << "not found!" << endl;
//        for(set<int>::iterator iter=location.begin();iter!=location.end();iter++){
//            cout << *iter << " ";
//        }
    return location;

}

//求基因的反向互补
string reverse_gene(string gene){
    int len = gene.size();
    for(int i=0;i<len;i++){
        switch(gene[i]){
        case 'A':
            gene[i] = 'T';break;
        case 'C':
            gene[i] = 'G';break;
        case 'G':
            gene[i] = 'C';break;
        case 'T':
            gene[i] = 'A';break;
        }
    }
    reverse(gene.begin(),gene.end());
    return gene;
}

//构建打分矩阵(罚分，都是正的）
void init_scoreMatrix(int mu,int deta){
    score_matrix[make_pair('-','A')]=deta;
    score_matrix[make_pair('A','-')]=deta;
    score_matrix[make_pair('-','C')]=deta;
    score_matrix[make_pair('C','-')]=deta;
    score_matrix[make_pair('-','G')]=deta;
    score_matrix[make_pair('G','-')]=deta;
    score_matrix[make_pair('-','T')]=deta;
    score_matrix[make_pair('T','-')]=deta;

    score_matrix[make_pair('A','A')]=0;
    score_matrix[make_pair('C','C')]=0;
    score_matrix[make_pair('G','G')]=0;
    score_matrix[make_pair('T','T')]=0;

    score_matrix[make_pair('A','C')]=deta;
    score_matrix[make_pair('A','G')]=deta;
    score_matrix[make_pair('A','T')]=deta;
    score_matrix[make_pair('C','A')]=deta;
    score_matrix[make_pair('C','G')]=deta;
    score_matrix[make_pair('C','T')]=deta;
    score_matrix[make_pair('G','C')]=deta;
    score_matrix[make_pair('G','A')]=deta;
    score_matrix[make_pair('G','T')]=deta;
    score_matrix[make_pair('T','C')]=deta;
    score_matrix[make_pair('T','G')]=deta;
    score_matrix[make_pair('T','A')]=deta;
}

//返回得分矩阵中的得分
int scoreMatrix(char first,char second){
    return score_matrix[make_pair(first,second)];
}

//用动态规划进行双序列的alignment
int score_alignment(string refer,string seq){
    //因为不需要比对的过程，只需要给该比对打分，所以采用只记录上一列的分数，每一列从上往下打分
    int len1 = refer.size();
    int len2 = seq.size();
    int last[len1];
    int cur[len1];
    //先初始化第一列
    last[0]=scoreMatrix(refer[0],seq[0]);
    for(int i=1;i<len1;i++){
        last[i]=last[i-1]+scoreMatrix(refer[i],'-');
    }

    //依次计算动态规划矩阵的每一列
    int min_score;
    for(int i=1;i<len2;i++){//i代表正在计算的seq中的字符位置
        cur[0]=last[0]+scoreMatrix('-',seq[i]);
        char c=seq[i];
        for(int j=1;j<len1;j++){//j代表正在计算的refer中的字符位置
            min_score=min(last[j-1]+scoreMatrix(refer[j],c),last[j]+scoreMatrix('-',c));
            cur[j] = min(min_score,cur[j-1]+scoreMatrix(refer[j],'-'));
        }
        memcpy(last,cur,len1*sizeof(int));
    }
    return cur[len1-1];
}

//SNAP algorithm,返回的第一个数为匹配的位置，第二个数为该次匹配的罚分
pair<int,int> SNAP(string read,int seed_size,int n,int d_max,int c,int h_max){
    int d_best = 2147483647;
    int d_second = 2147483647;
    map<int,int> seeds_hitting;
    int non_overlapping =0;
    int p,best_location=-1;
    set<int> scored_locations;
    //找所有种子在whole_gene中的最优匹配和次优匹配
    string seeds[n];
    for(int i=0;i<n;i++){
        seeds[i]=read.substr(i*seed_size,seed_size);
        if(hashtable.find(seeds[i])==hashtable.end()){//没有命中
            non_overlapping += 1;
        }
        else{
            set<int> locations=hashtable[seeds[i]];
            if(locations.size()<=(unsigned)h_max){
                //如果命中，对该种子命中的所有位置 的命中种子数+1
                for(set<int>::iterator it=locations.begin();it!=locations.end();it++){
                    p = *it - i*seed_size;
                    if(seeds_hitting.find(p)==seeds_hitting.end()){
                        seeds_hitting[p] = 1;
                    }
                    else seeds_hitting[p] += 1;
                }
                //找到当前未打分的位置中命中种子数最多的位置
                //认为这个位置就是当前匹配的最好的位置
                int tmp=0;
                for(map<int,int>::iterator iter=seeds_hitting.begin();iter!=seeds_hitting.end();iter++){
                    if(scored_locations.count(iter->first)==0){
                        if((iter->second)>tmp){
                            tmp = iter->second;
                            p = iter->first;
                        }
                    }
                }

                //给p位置打分，在此省略为降低时间复杂度而设置d_limit的过程
                string refer(whole_gene+p,min(whole_gene+p+read.size(),whole_gene+n_real));
                int d = score_alignment(read,refer);
                scored_locations.insert(p);
                //更新d_best和d_second
                if(d<=d_best){
                    d_second = d_best;
                    d_best = d; best_location = p;
                }
                //判断是否满足结束条件
                if(d_best==0 ||(d_best<c && d_second<d_best+c)){
                    //此时已经确定不存在confident hit
                    return make_pair(best_location,d_best);
                }
                else if(i+1>=d_best+c){
                    //剩余未命中的位置的错误已经足够多，不需要再评估了
                    //只需要判断当前在seeds_hitting中已经存在的位置即可。
                    for(map<int,int>::iterator ite=seeds_hitting.begin();ite!=seeds_hitting.end();ite++){
                        if(scored_locations.count(ite->first)==0){
                            string re(whole_gene+ite->first,min(whole_gene+ite->first+read.size(),whole_gene+n_real));
                            d = score_alignment(read,re);
                            if(d<=d_best){
                                d_second = d_best;
                                d_best = d; best_location = p;
                            }
                        }
                    }
                    break;
                }
            }//end if
        }//end else
    }//end for
    if(d_best<=d_max && d_second>=d_best+c){
        return make_pair(best_location,d_best);
    }
    else if(d_best<=d_max){
        return make_pair(best_location,d_best);
    }
    else
        return make_pair(-1,0);
}

//pair-end sequencing
void pair_end_sequencing(string file1,string file2){
    ifstream f1,f2;
    ofstream f;
    string tmp;
    f1.open(file1,ios::in);
    f2.open(file2,ios::in);
    if(f1.fail()){
        cout << "can not open the file1." << endl;
        exit(1);
    }
    if(f2.fail()){
        cout << "can not open the file2." << endl;
        exit(1);
    }
    //SAM文件内容
    string ID1,ID2;
    string rname="ref";//因为大肠杆菌为原核生物，只有一条DNA，所以全为ref
    int pos1,pos2;//比对上的位置
    int score1,score2;
    string CIGAR="*";//暂且不判断变异类型
    string rnext1,rnext2;//如果完全相同为=,否则为*
    //Pnext的值和pos相交换即可
    int tlen1,tlen2;//if |pos1-pos2|>=len(read),tlen=end-start,else tlen=2len(read)-|end-start|
    string read1,read2;
    string qual1,qual2;
//    int num=0;
//    DWORD Start,End;

    f.open("result.sam",ios::out);
    do{
//        if(num%1000==0){
//            Start = timeGetTime();
//        }
//        num++;
        int flag1=3+64,flag2=3+128;//因为读入的对全部都是Pair-read
        if(f1.good()&&f2.good()){
            getline(f1,ID1);
            getline(f2,ID2);
            getline(f1,read1);
            getline(f2,read2);
            getline(f1,tmp);
            getline(f2,tmp);
            getline(f1,qual1);
            getline(f2,qual2);
            int len1 = read1.size();
            int len2 = read2.size();

            int start,_end;
            pair<int,int> re1=SNAP(read1,k,len1/k,10,5,30);
            int location1 = re1.first;
            if(location1==-1){//没有发现可靠的位置，认为是反链
                string read=reverse_gene(read1);
                flag1 += 16;
                re1 =SNAP(read,k,len1/k,10,5,30);
                pos1 = re1.first;
                score1 = re1.second;
                re1 =SNAP(read2,k,len2/k,10,5,30);//第二条链是正链
                flag2 += 32;
                pos2 = re1.first;
                score2 = re1.second;

                start = pos2;
                _end = pos1+len1;
                if(_end-start>=len1){
                    tlen1 = start - _end-1;
                    tlen2 = -tlen1;
                }
                else{
                    tlen2 = _end-start+1;
                    tlen1 = len1+len2-_end + start+1;
                }
            }
            else{//匹配成功
                flag1 += 32;
                flag2 += 16;
                pos1 = re1.first;
                score1 = re1.second;
                string read = reverse_gene(read2);
                re1 =SNAP(read,k,len2/k,10,5,30);//第二条链是反链
                pos2 = re1.first;
                score2 = re1.second;

                start = pos1;
                _end = pos2+len2;
                if(_end-start>=len1){
                    tlen1 = _end-start+1;
                    tlen2 = -tlen1;
                }
                else{
                    tlen1 = _end-start+1;
                    tlen2 = len1+len2-_end + start+1;
                }
            }
            if(score1==0) rnext1="=";
            else rnext1 = "*";
            if(score2==0) rnext2="=";
            else rnext2 = "*";
        }
        f << ID1 <<"\t"<<flag1<<"\t"<<rname<<"\t"<<pos1<<"\t"<<score1<<"\t"
        <<CIGAR<<"\t"<<rnext1<<"\t"<<pos2<<"\t"<<tlen1<<"\t"<<read1<<"\t"<<qual1<<endl;
        f << ID2 <<"\t"<<flag2<<"\t"<<rname<<"\t"<<pos2<<"\t"<<score2<<"\t"
        <<CIGAR<<"\t"<<rnext2<<"\t"<<pos1<<"\t"<<tlen2<<"\t"<<read2<<"\t"<<qual2<<endl;

//        if(num%1000==0){
//            End = timeGetTime();
//            cout << num << "    " << Start-End <<endl;
//        }
    }while(!f1.eof()&&!f2.eof());
    f1.close();
    f2.close();
}


int main()
{
    read_file("NC_008253.fna");
    //read_file("testtext.fna");
//    //cout << whole_gene << endl;
    //构建哈希表
    construct_hashtable();
    init_scoreMatrix(1,1);
    pair_end_sequencing("Ecoli_4x.fq1","Ecoli_4x.fq2");
    //pair_end_sequencing("test.fq1","test.fq2");
//    cout << SNAP("GCAACG",k,2,1,3,30).first;
//    for(map<string,set<int>>::iterator iter=hashtable.begin();iter!=hashtable.end();iter++){
//        cout << (iter->second).size() << "    " ;
//        for(set<int>::iterator it=(iter->second).begin();it!=(iter->second).end();it++){
//            cout << *it << " ";
//        }
//        cout << endl;
//    }
//
//    //exact match的基因搜索
//    string read = "GCTTTTCAT";
//    search_gene(read);
//    //比对字符串
//    init_scoreMatrix(1,1);
//    cout << score_alignment("ATCGTAC","ATGTTAT");
//    //测试SNAP函数
//    pair<int,int> result = SNAP("GGGGasdfghjh",4,3,4,4,4);
//    cout << result.first << "  " << result.second;
}
