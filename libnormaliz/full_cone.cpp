/*
 * Normaliz 2.7
 * Copyright (C) 2007-2011  Winfried Bruns, Bogdan Ichim, Christof Soeger
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//---------------------------------------------------------------------------

#include <stdlib.h>
#include <set>
#include <map>
#include <iostream>
#include <string>
#include <algorithm>
#include <time.h>
#include <deque>

#include "full_cone.h"
#include "vector_operations.h"
#include "lineare_transformation.h"
#include "list_operations.h"
#include "my_omp.h"

//---------------------------------------------------------------------------

namespace libnormaliz {
using namespace std;

//---------------------------------------------------------------------------
//private
//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::add_hyperplane(const size_t& new_generator, const FACETDATA & positive,const FACETDATA & negative){
    size_t k;
    
    // NEW: indgen is the index of the generator being inserted    
    
    // vector<Integer> hyperplane(hyp_size,0); // initialized with 0
    
    FACETDATA NewFacet; NewFacet.Hyp.resize(dim); NewFacet.GenInHyp.resize(nr_gen);
    
    Integer used_for_tests;
    if (test_arithmetic_overflow) {  // does arithmetic tests
        for (k = 0; k <dim; k++) {
            NewFacet.Hyp[k]=positive.ValNewGen*negative.Hyp[k]-negative.ValNewGen*positive.Hyp[k];
            used_for_tests =(positive.ValNewGen%overflow_test_modulus)*(negative.Hyp[k]%overflow_test_modulus)-(negative.ValNewGen%overflow_test_modulus)*(positive.Hyp[k]%overflow_test_modulus);
            if (((NewFacet.Hyp[k]-used_for_tests) % overflow_test_modulus)!=0) {
                errorOutput()<<"Arithmetic failure in Full_cone::add_hyperplane. Possible arithmetic overflow.\n";
                throw ArithmeticException();
            }
        }
    }
    else  {                      // no arithmetic tests
        for (k = 0; k <dim; k++) {
            NewFacet.Hyp[k]=positive.ValNewGen*negative.Hyp[k]-negative.ValNewGen*positive.Hyp[k];
        }
    }
    v_make_prime(NewFacet.Hyp);
    NewFacet.ValNewGen=0; // not really needed, only for completeness
    
    NewFacet.GenInHyp=positive.GenInHyp & negative.GenInHyp; // new hyperplane contains old gen iff both pos and neg do
    NewFacet.GenInHyp.set(new_generator);  // new hyperplane contains new generator
    
    if(pyr_level==0){
        #pragma omp critical(HYPERPLANE)
        Facets.push_back(NewFacet);
    }
    else
       Facets.push_back(NewFacet);
}


//---------------------------------------------------------------------------


template<typename Integer>
void Full_Cone<Integer>::find_new_facets(const size_t& new_generator){

    //to see if possible to replace the function .end with constant iterator since push-back is performed.

    // NEW: new_generator is the index of the generator being inserted

    size_t i,k,nr_zero_i;
    size_t subfacet_dim=dim-2; // NEW dimension of subfacet
    size_t facet_dim=dim-1; // NEW dimension of facet
    
    const bool tv_verbose = false; //verbose && Support_Hyperplanes.size()>10000; //verbose in this method call
    
        
    // preparing the computations
    list <FACETDATA*> l_Pos_Simp,l_Pos_Non_Simp;
    list <FACETDATA*> l_Neg_Simp,l_Neg_Non_Simp;
    list <FACETDATA*> l_Neutral_Simp, l_Neutral_Non_Simp;
    
    boost::dynamic_bitset<> Zero_Positive(nr_gen),Zero_Negative(nr_gen);

    bool simplex;
    bool ranktest;
    
    if (tv_verbose) verboseOutput()<<"transform_values: create SZ,Z,PZ,P,NS,N"<<endl<<flush;
    size_t ipos=0;
    
    typename list<FACETDATA>::iterator ii = Facets.begin();
    
    size_t listsize=Facets.size();

    for (size_t kk=0; kk<listsize; ++kk) {
        for(;kk > ipos; ++ipos, ++ii) ;
        for(;kk < ipos; --ipos, --ii) ;
        simplex=false;
        
        nr_zero_i=ii->GenInHyp.count();
        if(ii->ValNewGen>0)
            Zero_Positive|=ii->GenInHyp;
        else if(ii->ValNewGen<0)
            Zero_Negative|=ii->GenInHyp;        
        if (nr_zero_i==dim-1)
            simplex=true;
            
        if (ii->ValNewGen==0) {
            ii->GenInHyp.set(new_generator);  // Must be set explicitly !!
            if (simplex) {
                l_Neutral_Simp.push_back(&(*ii));
            }   else {
                l_Neutral_Non_Simp.push_back(&(*ii));
            }
        }
        else if (ii->ValNewGen>0) {
            if (simplex) {
                l_Pos_Simp.push_back(&(*ii));
            } else {
                l_Pos_Non_Simp.push_back(&(*ii));
            }
        } 
        else if (ii->ValNewGen<0) {
            if (simplex) {
                l_Neg_Simp.push_back(&(*ii));
            } else {
                l_Neg_Non_Simp.push_back(&(*ii));
            }
        }
    }
    
    boost::dynamic_bitset<> Zero_PN(nr_gen);
    Zero_PN=Zero_Positive & Zero_Negative;
    
    if (tv_verbose) verboseOutput()<<"transform_values: copy to vector"<<endl;

    size_t nr_PosSimp  = l_Pos_Simp.size();
    size_t nr_PosNSimp = l_Pos_Non_Simp.size();
    size_t nr_NegSimp  = l_Neg_Simp.size();
    size_t nr_NegNSimp = l_Neg_Non_Simp.size();
    size_t nr_NeuSimp  = l_Neutral_Simp.size();
    size_t nr_NeuNSimp = l_Neutral_Non_Simp.size();

    ranktest=false;
    if (nr_PosNSimp+nr_NegNSimp+nr_NeuNSimp > dim*dim*dim/6)
        ranktest=true;

    vector <FACETDATA*> Pos_Simp(nr_PosSimp);
    vector <FACETDATA*> Pos_Non_Simp(nr_PosNSimp);
    vector <FACETDATA*> Neg_Simp(nr_NegSimp);
    vector <FACETDATA*> Neg_Non_Simp(nr_NegNSimp);
    vector <FACETDATA*> Neutral_Simp(nr_NeuSimp);
    vector <FACETDATA*> Neutral_Non_Simp(nr_NeuNSimp);

    for (k = 0; k < Pos_Simp.size(); k++) {
        Pos_Simp[k]=l_Pos_Simp.front();
        l_Pos_Simp.pop_front();
    }

    for (k = 0; k < Pos_Non_Simp.size(); k++) {
        Pos_Non_Simp[k]=l_Pos_Non_Simp.front();
        l_Pos_Non_Simp.pop_front();
    }

    for (k = 0; k < Neg_Simp.size(); k++) {
        Neg_Simp[k]=l_Neg_Simp.front();
        l_Neg_Simp.pop_front();
    }

    for (k = 0; k < Neg_Non_Simp.size(); k++) {
        Neg_Non_Simp[k]=l_Neg_Non_Simp.front();
        l_Neg_Non_Simp.pop_front();
    }

    for (k = 0; k < Neutral_Simp.size(); k++) {
        Neutral_Simp[k]=l_Neutral_Simp.front();
        l_Neutral_Simp.pop_front();
    }

    for (k = 0; k < Neutral_Non_Simp.size(); k++) {
        Neutral_Non_Simp[k]=l_Neutral_Non_Simp.front();
        l_Neutral_Non_Simp.pop_front();
    }

    if (tv_verbose) verboseOutput()<<"PS "<<nr_PosSimp<<" P "<<nr_PosNSimp<<" NS "<<nr_NegSimp<<" N "<<nr_NegNSimp<<" ZS "<<nr_NeuSimp<<" Z "<<nr_NeuNSimp<<endl<<flush;

    if (tv_verbose) verboseOutput()<<"transform_values: fill multimap with subfacets of NS"<<endl<<flush;
    
    multimap < boost::dynamic_bitset<>, int> Neg_Subfacet_Multi;

    boost::dynamic_bitset<> zero_i(nr_gen);
    boost::dynamic_bitset<> subfacet(nr_gen);

    for (i=0; i<nr_NegSimp;i++){
        zero_i=Zero_PN & Neg_Simp[i]->GenInHyp;
        nr_zero_i=zero_i.count();
        
        if(nr_zero_i==subfacet_dim) // NEW This case treated separately
            Neg_Subfacet_Multi.insert(pair <boost::dynamic_bitset<>, int> (zero_i,i));
            
        else{       
            for (k =0; k<nr_gen; k++) {  //TODO use BOOST ROUTINE
                if(zero_i.test(k)) {              
                    subfacet=zero_i;
                    subfacet.reset(k);  // remove k-th element from facet to obtain subfacet
                    Neg_Subfacet_Multi.insert(pair <boost::dynamic_bitset<>, int> (subfacet,i));
                }
            }
        }
    }


    if (tv_verbose) verboseOutput()<<"transform_values: go over multimap of size "<< Neg_Subfacet_Multi.size() <<endl<<flush;

    multimap < boost::dynamic_bitset<>, int > ::iterator jj;
    multimap < boost::dynamic_bitset<>, int > ::iterator del;
    jj =Neg_Subfacet_Multi.begin();                               // remove negative subfecets shared
    while (jj!= Neg_Subfacet_Multi.end()) {                       // by two neg simpl facets
        del=jj++;
        if (jj!=Neg_Subfacet_Multi.end() && (*jj).first==(*del).first) {   //delete since is the intersection of two negative simplicies
            Neg_Subfacet_Multi.erase(del);
            del=jj++;
            Neg_Subfacet_Multi.erase(del);
        }
    }

    if (tv_verbose) verboseOutput()<<"transform_values: singlemap size "<<Neg_Subfacet_Multi.size()<<endl<<flush;
    
    size_t nr_NegSubfMult = Neg_Subfacet_Multi.size();
    size_t nr_NegSubf;
    map < boost::dynamic_bitset<>, int > Neg_Subfacet;
    
    #pragma omp parallel private(jj)
    {
    size_t i,j,k,t,nr_zero_i;
    boost::dynamic_bitset<> subfacet(dim-2);
    jj = Neg_Subfacet_Multi.begin();
    size_t jjpos=0;

    map < boost::dynamic_bitset<>, int > ::iterator last_inserted=Neg_Subfacet.begin(); // used to speedup insertion into the new map
    bool found;
    #pragma omp for schedule(dynamic)
    for (size_t j=0; j<nr_NegSubfMult; ++j) {             // remove negative subfacets shared
        for(;j > jjpos; ++jjpos, ++jj) ;                // by non-simpl neg or neutral facets 
        for(;j < jjpos; --jjpos, --jj) ;

        subfacet=(*jj).first;
        found=false; 
        for (i = 0; i <Neutral_Simp.size(); i++) {
            found=subfacet.is_subset_of(Neutral_Simp[i]->GenInHyp);
            if(found)
                break;
        }
        if (!found) {
            for (i = 0; i <Neutral_Non_Simp.size(); i++) {
                found=subfacet.is_subset_of(Neutral_Non_Simp[i]->GenInHyp);
                if(found)
                    break;                    
                }
            if(!found) {
                for (i = 0; i <Neg_Non_Simp.size(); i++) {
                    found=subfacet.is_subset_of(Neg_Non_Simp[i]->GenInHyp);
                    if(found)
                        break; 
                }
            }
        }
        if (!found) {
            if(pyr_level==0){
                #pragma omp critical(NEGATIVE_SUBFACET)
                {last_inserted=Neg_Subfacet.insert(last_inserted,*jj);}
            } else {
                last_inserted=Neg_Subfacet.insert(last_inserted,*jj);
            }
        }
    }
    
    #pragma omp single
    {nr_NegSubf = Neg_Subfacet.size();}

    #pragma omp single nowait
    if (tv_verbose) {
        verboseOutput()<<"transform_values: reduced map size "<<nr_NegSubf<<endl<<flush;
    } 
    #pragma omp single nowait
    {Neg_Subfacet_Multi.clear();}

    #pragma omp single nowait
    if (tv_verbose) {
        verboseOutput()<<"transform_values: PS vs NS"<<endl<<flush;
    }
    
    boost::dynamic_bitset<> zero_i(nr_gen);
    map <boost::dynamic_bitset<>, int> ::iterator jj_map;

    #pragma omp for schedule(dynamic) nowait           // Now matching positive and negative (sub)facets
    for (i =0; i<nr_PosSimp; i++){ //Positive Simp vs.Negative Simp
        zero_i=Pos_Simp[i]->GenInHyp & Zero_PN;
        nr_zero_i=zero_i.count();
        
        if (nr_zero_i==subfacet_dim) {                 // NEW slight change in logic. Positive simpl facet shared at most
            jj_map=Neg_Subfacet.find(zero_i);           // one subfacet with negative simpl facet
            if (jj_map!=Neg_Subfacet.end()) {
                add_hyperplane(new_generator,*Pos_Simp[i],*Neg_Simp[(*jj_map).second]);
                (*jj_map).second = -1;  // block subfacet in further searches
            }
        }
        if (nr_zero_i==facet_dim){    // now there could be more such subfacets. We make all and search them.      
            for (k =0; k<nr_gen; k++) {  // BOOST ROUTINE
                if(zero_i.test(k)) { 
                    subfacet=zero_i;
                    subfacet.reset(k);  // remove k-th element from facet to obtain subfacet
                    jj_map=Neg_Subfacet.find(subfacet);
                    if (jj_map!=Neg_Subfacet.end()) {
                        add_hyperplane(new_generator,*Pos_Simp[i],*Neg_Simp[(*jj_map).second]);
                        (*jj_map).second = -1;
                    }
                }
            }
        }
    }

    #pragma omp single nowait
    if (tv_verbose) {
        verboseOutput()<<"transform_values: NS vs P"<<endl;
    }

//  for (jj_map = Neg_Subfacet.begin(); jj_map != Neg_Subfacet.end(); ++jj_map)  //Neg_simplex vs. Pos_Non_Simp
    jj_map = Neg_Subfacet.begin();
    jjpos=0;
    #pragma omp for schedule(dynamic) nowait
    for (size_t j=0; j<nr_NegSubf; ++j) {
        for( ; j > jjpos; ++jjpos, ++jj_map) ;
        for( ; j < jjpos; --jjpos, --jj_map) ;

        if ( (*jj_map).second != -1 ) {  // skip used subfacets
            for (i = 0; i <Pos_Non_Simp.size(); i++) {
                if(jj_map->first.is_subset_of(Pos_Non_Simp[i]->GenInHyp)){
                    add_hyperplane(new_generator,*Pos_Non_Simp[i],*Neg_Simp[(*jj_map).second]);
                    break;
                }
            }
        }
    }
    
    #pragma omp single nowait
    if (tv_verbose) {
        verboseOutput()<<"transform_values: PS vs N"<<endl;
    }

    vector<size_t> key(nr_gen);
    size_t nr_missing;
    bool common_subfacet;
    #pragma omp for schedule(dynamic) nowait
    for (size_t i =0; i<nr_PosSimp; i++){ //Positive Simp vs.Negative Non Simp
        zero_i=Zero_PN & Pos_Simp[i]->GenInHyp;
        nr_zero_i=0;
        for(j=0;j<nr_gen && nr_zero_i<=facet_dim;j++)
            if(zero_i.test(j)){
                key[nr_zero_i]=j;
                nr_zero_i++;
            }        
        if(nr_zero_i>=subfacet_dim) {
            for (j=0; j<Neg_Non_Simp.size(); j++){ // search negative facet with common subfacet
                nr_missing=0; 
                common_subfacet=true;               
                for(k=0;k<nr_zero_i;k++) {
                    if(!Neg_Non_Simp[j]->GenInHyp.test(key[k])) {
                        nr_missing++;
                        if(nr_missing==2 || nr_zero_i==subfacet_dim) {
                            common_subfacet=false;
                            break;
                        }
                    }
                 }
                    
                 if(common_subfacet){                 
                    add_hyperplane(new_generator,*Pos_Simp[i],*Neg_Non_Simp[j]);
                    if(nr_zero_i==subfacet_dim) // only one subfacet can lie in negative hyperplane
                        break;
                 }
            }           
        }            
    } // PS vs N



    #pragma omp single nowait
    if (tv_verbose) {
        verboseOutput()<<"transform_values: P vs N"<<endl<<flush;
    }
    
    bool exactly_two;
    FACETDATA *hp_i, *hp_j, *hp_t; // pointers to current hyperplanes
    
    size_t missing_bound, nr_common_zero;
    boost::dynamic_bitset<> common_zero(nr_gen);
    vector<size_t> common_key(nr_gen);
    
    #pragma omp for schedule(dynamic) nowait
    for (size_t i =0; i<nr_PosNSimp; i++){ //Positive Non Simp vs.Negative Non Simp

        hp_i=Pos_Non_Simp[i];
        zero_i=Zero_PN & hp_i->GenInHyp;
        nr_zero_i=0;
        for(j=0;j<nr_gen;j++)
            if(zero_i.test(j)){
                key[nr_zero_i]=j;
                nr_zero_i++;
            }
            
        if (nr_zero_i>=subfacet_dim) {
        
            missing_bound=nr_zero_i-subfacet_dim; // at most this number of generators can be missing
                                                  // to have a chance for common subfacet
                for (j=0; j<nr_NegNSimp; j++){
                hp_j=Neg_Non_Simp[j];
                
                nr_missing=0; 
                nr_common_zero=0;
                common_subfacet=true;               
                for(k=0;k<nr_zero_i;k++) {
                    if(!hp_j->GenInHyp.test(key[k])) {
                        nr_missing++;
                        if(nr_missing>missing_bound || nr_zero_i==subfacet_dim) {
                            common_subfacet=false;
                            break;
                        }
                    }
                    else {
                        common_key[nr_common_zero]=key[k];
                        nr_common_zero++;
                    }
                 }
                 

                if(common_subfacet){//intersection of *i and *j may be a subfacet
                    common_zero=zero_i & hp_j->GenInHyp;
                    exactly_two=true;

                    if (ranktest) {
                        Matrix<Integer> Test(nr_common_zero,dim);
                        for (k = 0; k < nr_common_zero; k++)
                            Test.write(k,Generators.read(common_key[k])); 

                        if (Test.rank_destructive()<subfacet_dim) {
                            exactly_two=false;
                        }
                    } // ranktest
                    else{                 // now the comparison test
                        for (t=0;t<nr_PosNSimp;t++){
                            hp_t=Pos_Non_Simp[t];
                            if (t!=i && common_zero.is_subset_of(hp_t->GenInHyp)) {                                
                                exactly_two=false;
                                break;
                            }
                        }
                        if (exactly_two) {
                            for (t=0;t<Neg_Non_Simp.size();t++){
                                hp_t=Neg_Non_Simp[t];
                                if (t!=j && common_zero.is_subset_of(hp_t->GenInHyp)) {  
                                    exactly_two=false;
                                    break;
                                }
                            }
                        }
                        if (exactly_two) {
                            for (t=0;t<nr_NeuNSimp;t++){
                                hp_t=Neutral_Non_Simp[t];
                                if(common_zero.is_subset_of(hp_t->GenInHyp)) {  
                                    exactly_two=false;
                                    break;
                                }
                            }
                            
                        }                        
                    } // else
                    if (exactly_two) {  //intersection of i and j is a subfacet
                        add_hyperplane(new_generator,*hp_i,*hp_j);
                    }
                }
            }
        }
    }
    } //END parallel

    //removing the negative hyperplanes
    // now done in build_cone

    if (tv_verbose) verboseOutput()<<"transform_values: done"<<endl;
}


//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::extend_triangulation(const size_t& new_generator){

    size_t listsize = Facets.size();
    vector<typename list<FACETDATA>::iterator> visible;
    visible.reserve(listsize);
    typename list<FACETDATA>::iterator i = Facets.begin();

    
    // #pragma omp critical(VERBOSE)
    // verboseOutput() << "L " << pyr_level << " H " << listsize << " T " << TriangulationSize << endl << flush;

    for (; i!=Facets.end(); ++i) 
        if (i->ValNewGen < 0) // visible facet
            visible.push_back(i);

    listsize = visible.size();
    // cout << "Pyr Level " << pyr_level << " Visible " << listsize <<  " Triang " << TriangulationSize << endl;


    typename list<SHORTSIMPLEX>::iterator oldTriBack = --Triangulation.end();
    #pragma omp parallel private(i)
    {
    size_t k,l;
    bool one_not_in_i, not_in_facet;
    size_t not_in_i=0;
    list<SHORTSIMPLEX> Triangulation_kk;
    SHORTSIMPLEX newsimplex;
    newsimplex.key.reserve(dim);
    bool Simpl_available;
    int tn = omp_get_ancestor_thread_num(1);
    
    typename list<SHORTSIMPLEX>::iterator F;
    typename list<SHORTSIMPLEX>::iterator j;
    
    vector<size_t> key(dim);
    
    #pragma omp for schedule(dynamic)
    for (size_t kk=0; kk<listsize; ++kk) {
        i=visible[kk];

        if (i->GenInHyp.count()==dim-1){  // simplicial
            l=0;
            for (k = 0; k <nr_gen; k++) {
                if (i->GenInHyp[k]==1) {
                    key[l]=k;
                    l++;
                }
            }
            key[dim-1]=new_generator;
            store_key(key,-i->ValNewGen);  // height understood positive
            continue;                      // done with simplicial cone facet
        }
        
        size_t irrelevant_vertices=0;
        for(size_t vertex=0;vertex<VertInTri.size();++vertex){
        
            if(i->GenInHyp[VertInTri[vertex]]==0) // lead vertex not in hyperplane
                continue;
                
            if(irrelevant_vertices<dim-2){
                ++irrelevant_vertices;
                continue;
            }
        
            typename list<SHORTSIMPLEX>::iterator  sectionEnd=TriSectionLast[vertex];
            ++sectionEnd;
            for(j=TriSectionFirst[vertex]; j!=sectionEnd;++j)
            {
              key=j->key;
              one_not_in_i=false;  // true indicates that one gen of simplex is not in hyperplane
              not_in_facet=false;  // true indicates that a second gen of simplex is not in hyperplane
              for(k=0;k<dim;k++)
              {
                 if ( !i->GenInHyp.test(key[k])) {
                     if(one_not_in_i){
                         not_in_facet=true;
                         break;
                     }
                     one_not_in_i=true;
                     not_in_i=k;
                  }
              }
              
              if(not_in_facet) // simplex does not share facet with hyperplane
                 continue;
              
              key[not_in_i]=new_generator;
              // store_key(key,-i->ValNewGen); // store simplex in triangulation
              
              newsimplex.key=key;
              newsimplex.height=-i->ValNewGen;
              
              if(keep_triangulation){
                Triangulation_kk.push_back(newsimplex);
                #pragma omp atomic
                TriangulationSize++;
                continue;   
              }
              
              Simpl_available=true;

              if(Top_Cone->FS[tn].empty()){
                  #pragma omp critical(FREESIMPL)
                  {
                  if(Top_Cone->FreeSimpl.empty())
                      Simpl_available=false;
                  else{
                      F=Top_Cone->FreeSimpl.begin();  // take 100 simplices from FreeSimpl
                      size_t q; for(q=0;q<1000;++q){            // or what you can get
                          if(F==Top_Cone->FreeSimpl.end())
                              break;
                          ++F;
                      }
                  
                      // cout << "Hole " << q << " " << tn << " " << omp_get_level() << " p " << omp_in_parallel() << endl;
                      if(q<1000)
                          Top_Cone->FS[tn].splice(Top_Cone->FS[tn].begin(),
                              Top_Cone->FreeSimpl);
                      else
                          Top_Cone->FS[tn].splice(Top_Cone->FS[tn].begin(),
                                        Top_Cone->FreeSimpl,Top_Cone->FreeSimpl.begin(),++F);
                  } // else
                  } // critical
              } // if empty
                    

              if(Simpl_available){
                  Triangulation_kk.splice(Triangulation_kk.end(),Top_Cone->FS[tn],
                                  Top_Cone->FS[tn].begin());
                  Triangulation_kk.back()=newsimplex;
              }
              else
                  Triangulation_kk.push_back(newsimplex); 
              
              #pragma omp atomic
              TriangulationSize++;
            
            } // for j
            
            if(pyr_level==0) {
                #pragma omp critical(TRIANG)
                Triangulation.splice(Triangulation.end(),Triangulation_kk);
            } else {
                Triangulation.splice(Triangulation.end(),Triangulation_kk);
            }
            
        } // for vertex
    } // for kk 
    } // parallel

    VertInTri.push_back(new_generator);
    TriSectionFirst.push_back(++oldTriBack);
    TriSectionLast.push_back(--Triangulation.end());
} 

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::store_key(const vector<size_t>& key, const Integer& height){

    SHORTSIMPLEX newsimplex;
    newsimplex.key=key;
    newsimplex.height=height;
    
    #pragma omp atomic
    TriangulationSize++;
    
    // if FreeSimpl is empty we (most likely) have to push a new element anyway
    if (keep_triangulation || Top_Cone->FreeSimpl.empty()){
        if (pyr_level==0) {
            #pragma omp critical(TRIANG)
            Triangulation.push_back(newsimplex);
        } else {
            Triangulation.push_back(newsimplex);
        }
        return;    
    }

    #pragma omp critical(FREESIMPL)
    {
    if (Top_Cone->FreeSimpl.empty()) { //this should not happen often
        #pragma omp critical(TRIANG)
        Triangulation.push_back(newsimplex);
    }
    else {
        Top_Cone->FreeSimpl.front()=newsimplex;
        if(pyr_level==0){
            #pragma omp critical(TRIANG)
            Triangulation.splice(Triangulation.end(),Top_Cone->FreeSimpl,Top_Cone->FreeSimpl.begin());
           } else {
            Triangulation.splice(Triangulation.end(),Top_Cone->FreeSimpl,Top_Cone->FreeSimpl.begin());
        }
    }
    }
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::process_pyramids(const size_t new_generator,const bool recursive){

    
    if(recursive)                               // in case we have to store new pyramids
        Top_Cone->Pyramids.resize(pyr_level+2); // in non-recursive mode done some where else
          
    vector<size_t> Pyramid_key;
    Pyramid_key.reserve(nr_gen);
    boost::dynamic_bitset<> in_Pyramid(nr_gen); 
     
    bool skip_remaining, skip_remaining_priv;
    typename list< FACETDATA >::iterator l;
    size_t i,lpos, listsize=Facets.size();
    //use deque here to have independent entries
    deque<bool> done(listsize,false);
    
    size_t Done=0;

    do{    

    lpos=0;
    skip_remaining=false;
    l=Facets.begin();

    #pragma omp parallel for private(i,skip_remaining_priv) firstprivate(lpos,l,Pyramid_key,in_Pyramid) schedule(dynamic) 
    for (size_t k=0; k<listsize; k++) {
    

        // #pragma omp critical(PYRPAR)
        skip_remaining_priv=skip_remaining;  //private copy
        
        if(skip_remaining_priv)
            continue;
            
        for(;k > lpos; lpos++, l++) ;
        for(;k < lpos; lpos--, l--) ;
                
        if(done[lpos])
            continue;
            
        //#pragma omp critical(DONE) //TODO change type to prevent side effects
        done[lpos]=true;
        
        #pragma omp atomic 
        Done++;
            
        if(l->ValNewGen>=0 ||(!recursive && do_partial_triangulation && l->ValNewGen>=-1)){
            continue;
        }
            
        boost::dynamic_bitset<> in_Pyramid(nr_gen,false);           
        Pyramid_key.push_back(new_generator);
        in_Pyramid.set(new_generator);
        for(i=0;i<nr_gen;i++){
            if(in_triang[i] && v_scalar_product(l->Hyp,Generators.read(i))==0){ // from the second extension on the incidence data
                Pyramid_key.push_back(i);                                      // in Facets is no longer up-to-date
                in_Pyramid.set(i);
            }
        }
        
        process_pyramid(Pyramid_key, in_Pyramid, new_generator, recursive);
        Pyramid_key.clear();
        
        if(check_evaluation_buffer_size() && omp_get_level()==1 && Done < listsize){  // we interrupt parallel execution if it is really parallel
                                                           //  to keep the triangulation buffer under control
            // #pragma omp critical(PYRPAR)
            skip_remaining=true;
        }
            
    }
    
    if(skip_remaining)
    {
        // cout << "Evaluate Process Skip " << endl;
           Top_Cone->evaluate_triangulation();
    }
    
    
    }while(skip_remaining);
        
    if(check_evaluation_buffer()){
        // cout << "Evaluate Process Ende " << endl;
          Top_Cone->evaluate_triangulation();
     }  
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::process_pyramid(const vector<size_t> Pyramid_key, const boost::dynamic_bitset<> in_Pyramid, 
                          const size_t new_generator,const bool recursive){
    
    #pragma omp atomic
    Top_Cone->totalNrPyr++;
    
    // cout << "recursive " << recursive << " PyrTiefe " << Top_Cone->Pyramids.size() << " PyrLevel " << pyr_level << endl;

    list<vector<Integer> > NewFacets;
    
    if(Pyramid_key.size()==dim){  // simplicial pyramid done here
        #pragma omp atomic
        Top_Cone->nrSimplicialPyr++;
        Simplex<Integer> S(Pyramid_key, Generators);
        if(recursive){ // the facets may be facets of the mother cone and if recursive==true must be given back
            Matrix<Integer> H=S.read_support_hyperplanes();
            for (size_t i=0; i<dim;i++)
                NewFacets.push_back(H.read(i));
        }    
        if(do_triangulation || (do_partial_triangulation && S.read_volume()>1)){
            if(!is_pyramid) {
                #pragma omp critical(EVALUATE) // nur auf Top-Ebene kritisch
                store_key(Pyramid_key,S.read_volume());  // height understood positive
            } else {
                store_key(Pyramid_key,S.read_volume());  // height understood positive
            }
        }
    }
    else {  // non-simplicial
        if(recursive){
            Full_Cone<Integer> Pyramid(*this,Pyramid_key);
            
            // the next line has the effect that we fully triangulate the pyramid in order to avoid
            // recursion down to simplices in case of partial triangulation
            Pyramid.do_triangulation= (do_partial_triangulation && pyr_level >=1) || do_triangulation;
            
            // cout << "In py " << omp_get_level() << " " << omp_get_thread_num() << " tn " << Pyramid.thread_num << endl;
            
            if(Pyramid.do_triangulation)
                Pyramid.do_partial_triangulation=false;
            Pyramid.build_cone(); // build and evaluate pyramid
       
            NewFacets.splice(NewFacets.begin(),Pyramid.Support_Hyperplanes);
       }
       else{ // if rcursive==false we only store the pyramid
           vector<size_t> key_wrt_top(Pyramid_key.size());
           for(size_t i=0;i<Pyramid_key.size();i++)
                key_wrt_top[i]=Top_Key[Pyramid_key[i]];
           #pragma omp critical(STOREPYRAMIDS)    
           Top_Cone->Pyramids[pyr_level].push_back(key_wrt_top);
       }
    }   

    // now we give support hyperplanes back to the mother cone if only if
    // they are not done on the "mother" level if and only if recursive==true
    

    if(recursive){
        size_t i;                        
        typename list<vector<Integer> >::iterator pyr_hyp = NewFacets.begin();
        bool new_global_hyp;
        FACETDATA NewFacet;
        Integer test;   
        for(;pyr_hyp!= NewFacets.end();pyr_hyp++){
            if(v_scalar_product(Generators.read(new_generator),*pyr_hyp)>0)
                continue;
            new_global_hyp=true;
            for(i=0;i<nr_gen;i++){
                if(in_Pyramid.test(i) || !in_triang[i])
                    continue;
                test=v_scalar_product(Generators.read(i),*pyr_hyp);
                if(test<=0){
                    new_global_hyp=false;
                    break;
                }
            
            }
            if(new_global_hyp){
                NewFacet.Hyp=*pyr_hyp;                
                if(pyr_level==0){
                    #pragma omp critical(HYPERPLANE) 
                    Facets.push_back(NewFacet);
                } else {
                    Facets.push_back(NewFacet);
                }
            }
        }
    }
    NewFacets.clear();
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::find_and_evaluate_start_simplex(){

    size_t i,j;
    Integer factor;

    
    Simplex<Integer> S = find_start_simplex();
    vector<size_t> key=S.read_key();   // generators indexed from 1
        
    // vector<bool> in_triang(nr_gen,false);
    for (i = 0; i < dim; i++) {
        in_triang[key[i]]=true;
    }
       
    Matrix<Integer> H=S.read_support_hyperplanes();
    for (i = 0; i <dim; i++) {
        FACETDATA NewFacet; NewFacet.Hyp.resize(dim); NewFacet.GenInHyp.resize(nr_gen);
        NewFacet.Hyp=H.read(i);
        for(j=0;j < dim;j++)
            if(j!=i)
                NewFacet.GenInHyp.set(key[j]);
        NewFacet.ValNewGen=-1;         // must be taken negative since opposite facet
        Facets.push_back(NewFacet);    // was visible before adding this vertex
    }
    
    if(!is_pyramid)
        Order_Vector = vector<Integer>(dim,0);
    
    if(!is_pyramid && do_h_vector){
        //define Order_Vector, decides which facets of the simplices are excluded
        Matrix<Integer> G=S.read_generators();
        //srand(12345);
        for(i=0;i<dim;i++){
            factor=(unsigned long)(2*(rand()%(2*dim))+3);
            for(j=0;j<dim;j++)
                Order_Vector[j]+=factor*G.read(i,j);        
        }
    }

    //the volume is an upper bound for the height

    
    if(do_triangulation || (do_partial_triangulation && S.read_volume()>1))
    {
        store_key(key,S.read_volume());  // height understood positive
    }
    
    if(do_triangulation){ // we must prepare the sections of the triangulation
        for(i=0;i<dim;i++)
        {
            VertInTri.push_back(key[i]);
            TriSectionFirst.push_back(Triangulation.begin());
            TriSectionLast.push_back(Triangulation.begin());
        }
    }
    
}



//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::evaluate_stored_pyramids(const size_t level){

    if(Pyramids[level].empty())
        return;
    Pyramids.resize(level+2); // make room for a new generation

    size_t nrDone=0;
    size_t nrPyramids=Pyramids[level].size();
    vector<short> Done(nrPyramids,0);
    
    cout << "************************************************" << endl;
    cout << "Evaluating " << nrPyramids << " pyramids on level " << level << endl;
    cout << "************************************************" << endl;
    
    typename list<vector<size_t> >::iterator p;
    size_t ppos;
    bool skip_remaining;

    do
    {

       p = Pyramids[level].begin();
       ppos=0;
       skip_remaining=false;
    
       #pragma omp parallel for firstprivate(p,ppos) schedule(dynamic) 
       for(size_t i=0; i<nrPyramids; i++){
       
           if(skip_remaining)
                continue;
                
           for(; i > ppos; ++ppos, ++p) ;
           for(; i < ppos; --ppos, --p) ;
           
           if(Done[i])
               continue;
           Done[i]=1;
           
           #pragma omp atomic
           nrDone++;
           
           /* #pragma omp critical(DONE)
           {
           nrDone++;
           cout << "Done " << nrDone << "To Evaluate " << Top_Cone->Triangulation.size() << endl;
           } */
            
           Full_Cone<Integer> Pyramid(*this,*p);
           Pyramid.recursion_allowed=false;
           Pyramid.pyr_level=level+1;
           if(level>=2){
               Pyramid.do_triangulation=true;
               Pyramid.do_partial_triangulation=false;
           }
           Pyramid.build_cone();
           if(check_evaluation_buffer_size() && nrDone < nrPyramids)  // we interrupt parallel execution if it is really parallel
                skip_remaining=true;                         //  to keep the triangulation buffer under control
                
       }
       
       if(skip_remaining){
       // cout << "Evaluate Stored Skip " << endl;
          Top_Cone->evaluate_triangulation();
       }
    
     }while(skip_remaining);
     
     if(check_evaluation_buffer())
     {
        // cout << "Evaluate Stored Ende " << endl;
          Top_Cone->evaluate_triangulation();
     }
     
     Pyramids[level].clear();
     evaluate_stored_pyramids(level+1);  
     
}

//---------------------------------------------------------------------------

/* builds the cone successively by inserting generators, computes all essential data
except global reduction */
template<typename Integer>
void Full_Cone<Integer>::build_cone() {
    if(dim>0){            //correction needed to include the 0 cone;
    if (verbose && !is_pyramid) {
        verboseOutput()<<"\n************************************************************\n";
        verboseOutput()<<"starting primal algorithm ";
        if (do_partial_triangulation) verboseOutput()<<"with partial triangulation ";
        if (do_triangulation) {
            verboseOutput()<<"with full triangulation ";
            if (!keep_triangulation) verboseOutput()<<"and direct evaluation ";
        }
        if (!do_triangulation && !do_partial_triangulation) verboseOutput()<<"(only support hyperplanes) ";
        verboseOutput()<<"..."<<endl;
    }
    size_t i;
    
    bool supphyp_recursion=false;
    bool tri_recursion=false;

    // DECIDE WHETHER TO USE RECURSION
    size_t RecBoundSuppHyp = dim*dim*dim;
    RecBoundSuppHyp *= RecBoundSuppHyp * 10; //dim^6 * 10
    size_t bound_div = nr_gen-dim+1;
    if(bound_div > 3*dim) bound_div = 3*dim;
    RecBoundSuppHyp /= bound_div;

    size_t RecBoundTriang = 1000000;  // 1 Mio      5000000; // 5Mio

//if(!is_pyramid) cout << "RecBoundSuppHyp = "<<RecBoundSuppHyp<<endl;

    find_and_evaluate_start_simplex();
    
    Integer scalar_product;
    bool new_generator;

    for (i = 0; i < nr_gen; i++) {
    
        if(in_triang[i] || (isComputed(ConeProperty::ExtremeRays) && !Extreme_Rays[i]))
            continue;

        new_generator=false;

        typename list< FACETDATA >::iterator l=Facets.begin();

        size_t nr_pos=0; size_t nr_neg=0;
        vector<Integer> L;
        size_t old_nr_supp_hyps=Facets.size();                
        
        size_t lpos=0;
        #pragma omp parallel for private(L,scalar_product) firstprivate(lpos,l) reduction(+: nr_pos, nr_neg) schedule(dynamic)
        for (size_t k=0; k<old_nr_supp_hyps; k++) {
            for(;k > lpos; lpos++, l++) ;
            for(;k < lpos; lpos--, l--) ;

            L=Generators.read(i);
            scalar_product=v_scalar_product(L,(*l).Hyp);
            if (test_arithmetic_overflow && !v_test_scalar_product(L,(*l).Hyp,scalar_product,overflow_test_modulus)) {
                error_msg("error: Arithmetic failure in Full_cone::support_hyperplanes. Possible arithmetic overflow.\n");
                throw ArithmeticException();
            }
            
            l->ValPrevGen=l->ValNewGen;  // last new generator is now previous generator
            l->ValNewGen=scalar_product;
            if (scalar_product<0) {
                new_generator=true;
                nr_neg++;
            }
            if (scalar_product>0) {
                nr_pos++;
            }
        }  //end parallel for
        
        if(!new_generator)
            continue;
            
        // Magic Bounds to decide whether to use pyramids
        if( supphyp_recursion || (recursion_allowed && nr_neg*nr_pos>RecBoundSuppHyp)){  // go to pyramids because of supphyps
             if(check_evaluation_buffer()){
                // cout << "Evaluation Build Mitte" << endl;
                    Top_Cone->evaluate_triangulation();
            }   
            supphyp_recursion=true;
            process_pyramids(i,true); //recursive
        }
        else{
            if(tri_recursion || (do_triangulation 
                         && nr_neg*TriangulationSize > RecBoundTriang)){ // go to pyramids because of triangulation
                if(check_evaluation_buffer()){
                    Top_Cone->evaluate_triangulation();
                }   
                tri_recursion=true;
                process_pyramids(i,false); //non-recursive
            }
            else{  // no pyramids necesary
                if(do_partial_triangulation)
                    process_pyramids(i,false); // non-recursive
                if(do_triangulation)
                    extend_triangulation(i);
            }
            find_new_facets(i); // in the non-recursive case we must compute supphyps on this level
        }
        
        // removing the negative hyperplanes
        l=Facets.begin();
        for (size_t j=0; j<old_nr_supp_hyps;j++){
            if (l->ValNewGen<0) 
                l=Facets.erase(l);
            else 
                l++;
        }
        
        in_triang[i]=true;

        if (verbose && !is_pyramid) {
            verboseOutput() << "generator="<< i+1 <<" and "<<Facets.size()<<" hyperplanes... ";
            if(keep_triangulation)
                verboseOutput() << TriangulationSize << " simplices ";
            verboseOutput()<<endl;
        }
    }

    typename list<FACETDATA>::const_iterator IHV=Facets.begin();
    for(;IHV!=Facets.end();IHV++){
        Support_Hyperplanes.push_back(IHV->Hyp);
    }
    

    } // end if (dim>0)
    
    Facets.clear();
    is_Computed.set(ConeProperty::SupportHyperplanes);
    
    if(!is_pyramid)
    {
        for(size_t i=1;i<Pyramids.size();i++) // unite all pyramids created from the top cone 
            Pyramids[0].splice(Pyramids[0].begin(),Pyramids[i]); // at level 0
        evaluate_stored_pyramids(0);             // there may some of higher level if recursion was used
    }

    transfer_triangulation_to_top(); // transfer remaining simplices to top
    if(check_evaluation_buffer()){
        // cout << "Evaluating in build_cone at end, pyr level " << pyr_level << endl;
        // cout << "Evaluation Build Ende" << endl;
        Top_Cone->evaluate_triangulation();
    }
    
    if(!is_pyramid && !keep_triangulation) // force evaluation of remaining simplices
        // cout << " Top cone evaluation in build_cone" << endl;
        evaluate_triangulation();          // on top level

    if(!is_pyramid && keep_triangulation)  // in this case triangulation now complete
        is_Computed.set(ConeProperty::Triangulation);  // and stored 
               
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::extreme_rays_and_ht1_check() {
    check_pointed();
    if(!pointed) return;
    compute_extreme_rays();
    check_ht1_extreme_rays();
    check_ht1_generated();
    if(!isComputed(ConeProperty::LinearForm)
       && (do_ht1_elements || do_h_vector)){
        errorOutput() << "No grading specified and cannot find one. "
                      << "Disabling some computations!" << endl;
        do_ht1_elements = false;
        do_h_vector = false;
    }
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::set_degrees() {
    if(gen_degrees.size()==0 && isComputed(ConeProperty::LinearForm)) // now we set the degrees
    {
        gen_degrees.resize(nr_gen);
        vector<Integer> gen_degrees_Integer=Generators.MxV(Linear_Form);
        for (size_t i=0; i<nr_gen; i++) {
            assert(gen_degrees_Integer[i] > 0);
            gen_degrees[i] = explicit_cast_to_long(gen_degrees_Integer[i]);
        }
    }
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::sort_gens_by_degree() {
    if(gen_degrees.size()==0)
        return;
        
    cout << "Before sort" << endl;
    for(size_t k=0;k<nr_gen;k++)
        cout << gen_degrees[k] << " " ;
    cout << endl;

    list<vector<Integer> > genList;
    vector<Integer> v(dim+3);
    vector<Integer> w(dim);
    size_t i,j;
    
    for(i=0;i<nr_gen;i++){
        v[0]=gen_degrees[i];
        v[1]=i;                // keep the input order as far as possible
        w=Generators[i];
        for(j=0;j<dim;j++)
            v[j+2]=w[j];
        v[dim+2]=0;
        if(Extreme_Rays[i]) // after sorting we must recover the extreme rays
            v[dim+2]=1;
        genList.push_back(v);
    }
    genList.sort();
    
    cout << "Sorted" << endl;
    
    i=0;
    typename list<vector<Integer> >::iterator g=genList.begin();
    for(;g!=genList.end();++g){
        v=*g;
        gen_degrees[i]=explicit_cast_to_long<Integer>(v[0]);
        Extreme_Rays[i]=false;
        if(v[dim+2]>0)
            Extreme_Rays[i]=true;
        for(j=0;j<dim;j++)
            w[j]=v[j+2];
        Generators[i]=w;
        i++;
    }
    
        cout << "After sort" << endl;
    for(size_t k=0;k<nr_gen;k++)
        cout << gen_degrees[k] << " " ;
    cout << endl;
    
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::compute_support_hyperplanes(){
    if(isComputed(ConeProperty::SupportHyperplanes))
        return;
    bool save_tri      = do_triangulation;
    bool save_part_tri = do_partial_triangulation;
    bool save_HB       = do_Hilbert_basis;
    bool save_ht1_el   = do_ht1_elements;
    bool save_h_vector = do_h_vector;
    do_triangulation         = false;
    do_partial_triangulation = false;
    do_Hilbert_basis         = false; 
    do_ht1_elements          = false; 
    do_h_vector              = false;
    build_cone();
    do_triangulation         = save_tri;
    do_partial_triangulation = save_part_tri;
    do_Hilbert_basis         = save_HB;
    do_ht1_elements          = save_ht1_el;
    do_h_vector              = save_h_vector;
}

//---------------------------------------------------------------------------

template<typename Integer>
bool Full_Cone<Integer>::check_evaluation_buffer(){

    return(omp_get_level()==0 && check_evaluation_buffer_size());
}

//---------------------------------------------------------------------------

template<typename Integer>
bool Full_Cone<Integer>::check_evaluation_buffer_size(){

    size_t EvalBoundTriang = 1000000; // 1Mio

    return(!Top_Cone->keep_triangulation && 
               Top_Cone->TriangulationSize > EvalBoundTriang);
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::transfer_triangulation_to_top(){  // NEW EVA

    size_t i;

    // cout << "Pyr level " << pyr_level << endl;
    
    if(!is_pyramid) {  // we are in top cone
        if(check_evaluation_buffer()){
            evaluate_triangulation();
        }
        return;      // no transfer necessary
    }

    // now we are in a pyramid

    // cout << "In pyramid " << endl;
  
    typename list<SHORTSIMPLEX>::iterator pyr_simp=Triangulation.begin();
    for(;pyr_simp!=Triangulation.end();pyr_simp++)
        for(i=0;i<dim;i++)
            pyr_simp->key[i]=Top_Key[pyr_simp->key[i]];

    // cout << "Keys transferred " << endl;
    #pragma omp critical(EVALUATE)
    {
        Top_Cone->Triangulation.splice(Top_Cone->Triangulation.end(),Triangulation);
        Top_Cone->TriangulationSize+=TriangulationSize;
    }
    TriangulationSize  =  0;

    // cout << "Done." << endl;
  
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::evaluate_triangulation(){

    // #pragma omp critical(EVALUATE)
    if(TriangulationSize>0)
    {
    const long VERBOSE_STEPS = 50;
    long step_x_size = TriangulationSize-VERBOSE_STEPS;
    if (verbose) {
        verboseOutput() << "evaluating "<<TriangulationSize<<" simplices" <<endl;
        verboseOutput() << "---------+---------+---------+---------+---------+"
                        << " (one | per 2%)" << endl;
    }
    
    totalNrSimplices+=TriangulationSize;

    #pragma omp parallel 
    {
        typename list<SHORTSIMPLEX>::iterator s = Triangulation.begin();
        size_t spos=0;
        SimplexEvaluator<Integer> simp(*this);
        #pragma omp for schedule(dynamic) 
        for(size_t i=0; i<TriangulationSize; i++){
            for(; i > spos; ++spos, ++s) ;
            for(; i < spos; --spos, --s) ;

            s->height = simp.evaluate(s->key,s->height);
            if(keep_triangulation)
                sort(s->key.begin(),s->key.end());
            if (verbose) {
                #pragma omp critical(VERBOSE)
                while ((long)(i*VERBOSE_STEPS) >= step_x_size) {
                    step_x_size += TriangulationSize;
                    verboseOutput() << "|" <<flush;
                }
            }
            if(spos%20000==0)
            {
                #pragma omp critical(HT1ELEMENTS)
                {
                Ht1_Elements.sort();
                Ht1_Elements.unique();
                }
                #pragma omp critical(CANDIDATES)
                {
                Candidates.sort();
                Candidates.unique();
                }
            }
        }
        #pragma omp critical(MULTIPLICITY)
        multiplicity += simp.getMultiplicitySum(); 
    }
    
    HilbertSeries ZeroHS;
    
    for(int i=0;i<omp_get_max_threads();++i){
        Hilbert_Series+=HS[i];
        HS[i]=ZeroHS;
    }


    Ht1_Elements.sort();
    Ht1_Elements.unique();

    Candidates.sort();
    Candidates.unique();

    if (verbose)
    {
        verboseOutput() << endl << totalNrSimplices << " simplices";
        if(do_Hilbert_basis)
            verboseOutput() << ", " << Candidates.size() << " HB candidates";
        if(do_ht1_elements)
            verboseOutput() << ", " << Ht1_Elements.size()<< " ht1 vectors";
        verboseOutput() << " accumulated." << endl;
    }
    
    if(!keep_triangulation){
        // Triangulation.clear();
        #pragma omp critical(FREESIMPL)
        FreeSimpl.splice(FreeSimpl.begin(),Triangulation);       
        TriangulationSize=0;
    }
    
    
} // TriangulationSize

}
//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::primal_algorithm(){

    // if keep_triangulation==false we must first find a grading if it is needed
    if (!keep_triangulation && !isComputed(ConeProperty::LinearForm) && (do_triangulation || do_ht1_elements || do_h_vector)) {
        check_ht1_generated();
        if(!ht1_generated) {
            compute_support_hyperplanes();
            extreme_rays_and_ht1_check();
            if(!pointed) return;

            Support_Hyperplanes.clear();  // will be computed again by build_cone
            is_Computed.reset(ConeProperty::SupportHyperplanes);
            for(size_t i=0;i<nr_gen;i++)
                in_triang[i]=false;
        }
        //if keep_triangulation==false we should have a linear form if we need one and can find it
        if (!isComputed(ConeProperty::LinearForm)) {
            if (do_ht1_elements)
                return;
            if (do_h_vector)
                do_h_vector=false;
        }
    }
    set_degrees();
    sort_gens_by_degree();

    build_cone();  // evaluates if keep_triangulation==false
    
    cout << "TotPyr "<< totalNrPyr << endl;
    cout << "Uni "<< Unimod << " Ht1NonUni " << Ht1NonUni << " NonDecided " << NonDecided << " TotNonDec " << NonDecidedHyp<< endl;

    extreme_rays_and_ht1_check();
    if(!pointed) return;
    set_degrees();
    if (!isComputed(ConeProperty::LinearForm)) {
        if (do_ht1_elements)
            return;
        if (do_h_vector)
            do_h_vector=false;
    }

    if (keep_triangulation)
        evaluate_triangulation();
    FreeSimpl.clear();
    
    if (ht1_extreme_rays && do_triangulation)
        is_Computed.set(ConeProperty::Multiplicity,true);
        
    if (do_Hilbert_basis) {
        global_reduction();
        is_Computed.set(ConeProperty::HilbertBasis,true);
        check_integrally_closed();
    }
    
    if (isComputed(ConeProperty::LinearForm) && do_Hilbert_basis) {
        select_ht1_elements();
        check_ht1_hilbert_basis();
    }
    if (do_ht1_elements) {
        for(size_t i=0;i<nr_gen;i++)
            if(v_scalar_product(Linear_Form,Generators.read(i))==1)
                Ht1_Elements.push_front(Generators.read(i));
        Ht1_Elements.sort();
        Ht1_Elements.unique();
        is_Computed.set(ConeProperty::Ht1Elements,true);
    }
    if (do_h_vector) {
        Hilbert_Series.simplify();
        is_Computed.set(ConeProperty::HVector);
    }

}

   
//---------------------------------------------------------------------------
// Normaliz modes (public)
//---------------------------------------------------------------------------

// pure dualization
template<typename Integer>
void Full_Cone<Integer>::dualize_cone() {  
    compute_support_hyperplanes();
    reset_tasks();
}

// -s
template<typename Integer>
void Full_Cone<Integer>::support_hyperplanes() { 
    compute_support_hyperplanes();
    extreme_rays_and_ht1_check();
    reset_tasks();
}

// -v
template<typename Integer>
void Full_Cone<Integer>::support_hyperplanes_triangulation() {
    do_triangulation=true;
    keep_triangulation=true;
    primal_algorithm();
    reset_tasks();
}


// -V
template<typename Integer>
void Full_Cone<Integer>::support_hyperplanes_triangulation_pyramid() {   
    do_triangulation=true; 
    primal_algorithm();
    reset_tasks();
}

//-n
template<typename Integer>
void Full_Cone<Integer>::triangulation_hilbert_basis() {
    do_Hilbert_basis=true;
    do_triangulation=true;
    keep_triangulation=true;
    primal_algorithm();
    reset_tasks();
}

// -N
template<typename Integer>
void Full_Cone<Integer>::hilbert_basis() {
    do_Hilbert_basis=true;
    do_partial_triangulation=true;
    primal_algorithm();
    reset_tasks();
}

// -h
template<typename Integer>
void Full_Cone<Integer>::hilbert_basis_polynomial() {
    do_Hilbert_basis=true;
    do_h_vector=true;
    do_triangulation=true;
    keep_triangulation=true;
    primal_algorithm();
    reset_tasks();    
}

// -H
template<typename Integer>
void Full_Cone<Integer>::hilbert_basis_polynomial_pyramid() {
    do_Hilbert_basis=true;
    do_h_vector=true;
    do_triangulation=true;
    primal_algorithm();
    reset_tasks();    
}

// -p
template<typename Integer>
void Full_Cone<Integer>::hilbert_polynomial() {
    do_ht1_elements=true;
    do_h_vector=true;
    do_triangulation=true;
    keep_triangulation=true;
    primal_algorithm();
    reset_tasks();
}

// -P
template<typename Integer>
void Full_Cone<Integer>::hilbert_polynomial_pyramid() {
    do_ht1_elements=true;
    do_h_vector=true;
    do_triangulation=true;
    primal_algorithm();
    reset_tasks();
}

// -1
template<typename Integer>
void Full_Cone<Integer>::ht1_elements() {
    do_ht1_elements=true;
    do_partial_triangulation=true;
    primal_algorithm();
    reset_tasks();
}

template<typename Integer>
void Full_Cone<Integer>::dual_mode() {
    Support_Hyperplanes.sort();
    Support_Hyperplanes.unique();
    Support_Hyperplanes.remove(vector<Integer>(dim,0));

    if(dim>0) {            //correction needed to include the 0 cone;
        Linear_Form = Generators.homogeneous(ht1_extreme_rays);
        ht1_generated = ht1_extreme_rays;
        is_Computed.set(ConeProperty::IsHt1ExtremeRays);
        is_Computed.set(ConeProperty::IsHt1Generated);

        if (ht1_extreme_rays) {
            is_Computed.set(ConeProperty::LinearForm);
            if (verbose) { 
                cout << "Find height 1 elements" << endl;
            }
            typename list < vector <Integer> >::const_iterator h;
            for (h = Hilbert_Basis.begin(); h != Hilbert_Basis.end(); ++h) {
                if (v_scalar_product((*h),Linear_Form)==1) {
                    Ht1_Elements.push_back((*h));
                }
            }
            is_Computed.set(ConeProperty::Ht1Elements);
        }
    } else {
        ht1_extreme_rays = ht1_generated = true;
        Linear_Form=vector<Integer>(dim);
        is_Computed.set(ConeProperty::IsHt1ExtremeRays);
        is_Computed.set(ConeProperty::IsHt1ExtremeRays);
        is_Computed.set(ConeProperty::LinearForm);
    }
    if (isComputed(ConeProperty::LinearForm)) check_ht1_hilbert_basis();
    check_integrally_closed();
}

//---------------------------------------------------------------------------
// Checks and auxiliary algorithms
//---------------------------------------------------------------------------


template<typename Integer>
Simplex<Integer> Full_Cone<Integer>::find_start_simplex() const {
    if (isComputed(ConeProperty::ExtremeRays)) {
        vector<size_t> marked_extreme_rays(0);
        for (size_t i=0; i<nr_gen; i++) {
            if (Extreme_Rays[i])
                marked_extreme_rays.push_back(i);
        }
        vector<size_t> key_extreme = Generators.submatrix(Extreme_Rays).max_rank_submatrix_lex(dim);
        assert(key_extreme.size() == dim);
        vector<size_t> key(dim);
        for (size_t i=0; i<dim; i++) {
            key[i] = marked_extreme_rays[key_extreme[i]];
        }
        return Simplex<Integer>(key, Generators);
    } 
    else {
    // assert(Generators.rank()>=dim); 
        return Simplex<Integer>(Generators);
    }
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::compute_extreme_rays(){
    if (isComputed(ConeProperty::ExtremeRays))
        return;
    assert(isComputed(ConeProperty::SupportHyperplanes));
    size_t i,j,k,l,t;
    Matrix<Integer> SH=getSupportHyperplanes().transpose();
    Matrix<Integer> Val=Generators.multiplication(SH);
    size_t nc=Val.nr_of_columns();
    vector<size_t> Zero(nc);
    vector<size_t> nr_zeroes(nr_gen);

    for (i = 0; i <nr_gen; i++) {
        k=0;
        Extreme_Rays[i]=true;
        for (j = 0; j <nc; j++) {
            if (Val.get_elem(i,j)==0) {
                k++;
            }
        }
        nr_zeroes[i]=k;
        if (k<dim-1||k==nc)  // not contained in enough facets or in all (0 as generator)
            Extreme_Rays[i]=false;
    }

    for (i = 0; i <nr_gen; i++) {
        if(!Extreme_Rays[i])  // already known to be non-extreme
            continue;

        k=0;
        for (j = 0; j <nc; j++) {
            if (Val.get_elem(i,j)==0) {
                Zero[k]=j;
                k++;
            }
        }

        for (j = 0; j <nr_gen; j++) {
            if (i!=j && Extreme_Rays[j]                // not compare with itself or a known nonextreme ray
                     && nr_zeroes[i]<nr_zeroes[j]) {   // or something whose zeroes cannot be a superset
                l=0;
                for (t = 0; t < nr_zeroes[i]; t++) {
                    if (Val.get_elem(j,Zero[t])==0)
                        l++;
                    if (l>=nr_zeroes[i]) {
                        Extreme_Rays[i]=false;
                        break;
                    }
                }
            }
        }
    }

    is_Computed.set(ConeProperty::ExtremeRays);
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::select_ht1_elements() {

    typename list<vector<Integer> >::iterator h = Hilbert_Basis.begin();
    for(;h!=Hilbert_Basis.end();h++)
        if(v_scalar_product(Linear_Form,*h)==1)
            Ht1_Elements.push_back(*h);
    is_Computed.set(ConeProperty::Ht1Elements,true);
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::check_pointed() {
    assert(isComputed(ConeProperty::SupportHyperplanes));
    if (isComputed(ConeProperty::IsPointed))
        return;
    Matrix<Integer> SH = getSupportHyperplanes();
    pointed = (SH.rank_destructive() == dim);
    is_Computed.set(ConeProperty::IsPointed);
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::check_ht1_generated() {
    if (isComputed(ConeProperty::IsHt1Generated))
        return;

    if (isComputed(ConeProperty::ExtremeRays)) {
        check_ht1_extreme_rays();
        if (ht1_extreme_rays) {
            ht1_generated = true;
            for (size_t i = 0; i < nr_gen; i++) {
                if (!Extreme_Rays[i] && v_scalar_product(Generators[i], Linear_Form) != 1) {
                    ht1_generated = false;
                    break;
                }
            }
        } else {
            ht1_generated = false;
        }
    } else {
        if (isComputed(ConeProperty::LinearForm)) {
            Integer sp; //scalar product
            ht1_generated = true;
            for (size_t i = 0; i < nr_gen; i++) {
                sp = v_scalar_product(Generators[i], Linear_Form);
                if (sp<1) {
                    errorOutput() << "Linear form gives non-positive value " << sp
                                  << " for generator " << i+1 << "." << endl;
                    throw BadInputException();
                }
                if (sp != 1) {
                    ht1_generated = false;
                    break;
                }
            }
        } else {
            Linear_Form = Generators.homogeneous(ht1_generated);
            if (ht1_generated) {
                ht1_extreme_rays=true;
                is_Computed.set(ConeProperty::IsHt1ExtremeRays);
                is_Computed.set(ConeProperty::LinearForm);
            }
        }
    }
    is_Computed.set(ConeProperty::IsHt1Generated);
}

template<typename Integer>
void Full_Cone<Integer>::check_ht1_extreme_rays() {
    if (isComputed(ConeProperty::IsHt1ExtremeRays))
        return;

    if (ht1_generated) {
        ht1_extreme_rays=true;
        is_Computed.set(ConeProperty::IsHt1ExtremeRays);
        return;
    }
    assert(isComputed(ConeProperty::ExtremeRays));
    if (isComputed(ConeProperty::LinearForm)) {
        Integer sp; //scalar product
        ht1_extreme_rays = true;
        for (size_t i = 0; i < nr_gen; i++) {
            if (!Extreme_Rays[i])
                continue;
            sp = v_scalar_product(Generators[i], Linear_Form);
            if (sp<1) {
                errorOutput() << "Linear form gives non-positive value " << sp
                              << " for generator " << i+1 << "." << endl;
                throw BadInputException();
            }
            if (sp != 1) {
                ht1_extreme_rays = false;
                break;
            }
        }
    } else {
        Matrix<Integer> Extreme=Generators.submatrix(Extreme_Rays);
        Linear_Form = Extreme.homogeneous(ht1_extreme_rays);
        if (ht1_extreme_rays) {
            is_Computed.set(ConeProperty::LinearForm);
        }
    }
    is_Computed.set(ConeProperty::IsHt1ExtremeRays);
}

template<typename Integer>
void Full_Cone<Integer>::check_ht1_hilbert_basis() {
    if (isComputed(ConeProperty::IsHt1HilbertBasis))
        return;

    if ( !isComputed(ConeProperty::LinearForm) || !isComputed(ConeProperty::HilbertBasis)) {
        errorOutput() << "Warning: unsatisfied preconditions in check_ht1_hilbert_basis()!" <<endl;
        return;
    }
    
    if (isComputed(ConeProperty::Ht1Elements)) {
        ht1_hilbert_basis = (Ht1_Elements.size() == Hilbert_Basis.size());
    } else {
        ht1_hilbert_basis = true;
        typename list< vector<Integer> >::iterator h;
        for (h = Hilbert_Basis.begin(); h != Hilbert_Basis.end(); ++h) {
            if (v_scalar_product((*h),Linear_Form)!=1) {
                ht1_hilbert_basis = false;
                break;
            }
        }
    }
    is_Computed.set(ConeProperty::IsHt1HilbertBasis);
}

template<typename Integer>
void Full_Cone<Integer>::check_integrally_closed() {
    if (isComputed(ConeProperty::IsIntegrallyClosed))
        return;

    if ( !isComputed(ConeProperty::HilbertBasis)) {
        errorOutput() << "Warning: unsatisfied preconditions in check_integrally_closed()!" <<endl;
        return;
    }
    integrally_closed = false;
    if (Hilbert_Basis.size() <= nr_gen) {
        integrally_closed = true;
        typename list< vector<Integer> >::iterator h;
        for (h = Hilbert_Basis.begin(); h != Hilbert_Basis.end(); ++h) {
            integrally_closed = false;
            for (size_t i=0; i< nr_gen; i++) {
                if ((*h) == Generators.read(i)) {
                    integrally_closed = true;
                    break;
                }
            }
            if (!integrally_closed) {
                break;
            }
        }
    }
    is_Computed.set(ConeProperty::IsIntegrallyClosed);
}

//---------------------------------------------------------------------------
// Global reduction
//---------------------------------------------------------------------------

template<typename Integer>
bool Full_Cone<Integer>::is_reducible(list< vector<Integer>* >& Irred, const vector< Integer >& new_element){
    size_t i;
    size_t s=Support_Hyperplanes.size();
    vector <Integer> candidate=v_cut_front(new_element,dim);
    vector <Integer> scalar_product=l_multiplication(Support_Hyperplanes,candidate);
    typename list< vector<Integer>* >::iterator j;
    vector<Integer> *reducer;
    for (j =Irred.begin(); j != Irred.end(); j++) {
        reducer=(*j);
        for (i = 1; i <= s; i++) {
            if ((*reducer)[i]>scalar_product[i-1]){
                break;
            }
        }
        if (i==s+1) {
            //found a "reducer" and move it to the front
            Irred.push_front(*j);
            Irred.erase(j);
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::global_reduction() {
    Integer norm;
    
    list <vector<Integer> > Candidates_with_Scalar_Product;
    list <vector<Integer> > HB;
    typename list <vector<Integer> >::iterator c;
    typename list <vector<Integer> >::const_iterator h;
    typename list <vector<Integer> >::iterator cit;
    
    for (size_t i = 0; i <nr_gen; i++) {
        Candidates.push_front(Generators.read(i));
    }
    if(verbose) verboseOutput()<<"sorting the candidates... "<<flush;
    Candidates.sort();
    if(verbose) verboseOutput()<<"make them unique... "<<flush;
    Candidates.unique();
    if(verbose) verboseOutput()<<"done."<<endl;

    if (nr_gen == dim) { // cone is simplicial, therefore no global reduction is necessary
        if (verbose) {
            verboseOutput()<<"Cone is simplicial, no global reduction necessary."<<endl;
        }
        for (cit = Candidates.begin(); cit != Candidates.end(); ++cit) {
            Hilbert_Basis.push_back(v_cut_front(*cit,dim));
        }
        Candidates.clear();
        return;
    }
    

    vector<Integer> degree_function=compute_degree_function();

    cit = Candidates.begin();
    size_t cpos = 0;
    size_t listsize=Candidates.size();
    
    if(verbose) {
        verboseOutput()<<"computing the degrees of the candidates... "<<flush;
    }
    //go over candidates: do single scalar product
    //for (c = Candidates.begin(); c != Candidates.end(); c++) 
    vector<Integer> scalar_product;
    for (size_t j=0; j<listsize; ++j) {
        for(;j > cpos; ++cpos, ++cit) ;
        for(;j < cpos; --cpos, --cit) ;

        norm=v_scalar_product(degree_function,(*cit));

        vector <Integer> new_element(1, norm);
        new_element=v_merge(new_element,(*cit));
        Candidates_with_Scalar_Product.push_back(new_element);
    }
    Candidates.clear();         //delete old set
    if(verbose) {
        verboseOutput()<<"sorting the list... "<<endl<<flush;
    }
    Candidates_with_Scalar_Product.sort();
    if (verbose) {
        verboseOutput()<< Candidates_with_Scalar_Product.size() <<" candidate vectors sorted."<<endl;
    }
    
    // do global reduction
    list< vector<Integer> > HBtmp(0);
    Integer norm_crit;
    while ( !Candidates_with_Scalar_Product.empty() ) {
        //use norm criterion to find irreducible elements
        c=Candidates_with_Scalar_Product.begin();
        norm_crit=(*c)[0]*2;  //candidates with smaller norm are irreducible
        if ( Candidates_with_Scalar_Product.back()[0] < norm_crit) { //all candidates are irreducible
            if (verbose) {
                verboseOutput()<<Hilbert_Basis.size()+Candidates_with_Scalar_Product.size();
                verboseOutput()<<" Hilbert Basis elements of degree <= "<<norm_crit-1<<"; done"<<endl;
            }
            while ( !Candidates_with_Scalar_Product.empty()) {
                Hilbert_Basis.push_back(v_cut_front(*c,dim)); // already of the final type 
                c=Candidates_with_Scalar_Product.erase(c);
            }
            break;
        }
        while ( (*c)[0] < norm_crit ) { //can't go over the end because of the previous if
            // push to HBtmp with scalar products
            vector <Integer> candidate=v_cut_front(*c,dim);
            vector <Integer> scalar_products=l_multiplication(Support_Hyperplanes,candidate);
            vector <Integer> new_HB_element(1);
            new_HB_element[0]=(*c)[0];
            new_HB_element=v_merge(new_HB_element,scalar_products);
            new_HB_element=v_merge(new_HB_element,candidate);
            HBtmp.push_back(new_HB_element);
            Hilbert_Basis.push_back(candidate); // already of the final type 
            c=Candidates_with_Scalar_Product.erase(c);
        }
        size_t csize=Candidates_with_Scalar_Product.size();
        if (verbose) {
            verboseOutput()<<Hilbert_Basis.size()<< " Hilbert Basis elements of degree <= "<<norm_crit-1<<"; "<<csize<<" candidates left"<<endl;
        }

        // reduce candidates against HBtmp
        // fill pointer list
        list < vector <Integer>* >  HBpointers;  // used to put "reducer" to the front
        c=HBtmp.begin();
        while (c!=HBtmp.end()) {
            HBpointers.push_back(&(*(c++)));
        }

        long VERBOSE_STEPS = 50;      //print | for 2%
        if (verbose && csize>50000) { //print | for 1000 candidates
            VERBOSE_STEPS=csize/1000;
        }
        long step_x_size = csize-VERBOSE_STEPS;
        long counter = 0;
        long steps_done = 0;
        if (verbose) {
            verboseOutput() << "---------+---------+---------+---------+---------+";
            if (VERBOSE_STEPS == 50) {
                verboseOutput() << " (one | per 2%)" << endl;
            } else { 
                verboseOutput() << " (one | per 1000 candidates)" << endl;
            }
        }


        #pragma omp parallel private(c,cpos) firstprivate(HBpointers)
        {
        
    //  list< vector<Integer>* > HBcopy(HBpointers); //one copy for each thread

        c=Candidates_with_Scalar_Product.begin();
        cpos=0;
        #pragma omp for schedule(dynamic)
        for (size_t k=0; k<csize; ++k) {
            for(;k > cpos; ++cpos, ++c) ;
            for(;k < cpos; --cpos, --c) ;
            
            if ( is_reducible(HBpointers, *c) ) {
                (*c)[0]=-1; //mark as reducible
            }

            if (verbose) {
                #pragma omp critical(VERBOSE)
                {
                counter++;

                while (counter*VERBOSE_STEPS >= step_x_size) {
                    steps_done++;
                    step_x_size += csize;
                    verboseOutput() << "|" <<flush;
//                  cout<<counter<<" ";
                    if(VERBOSE_STEPS > 50 && steps_done%50 == 0) {
                        verboseOutput() << "  " << (steps_done) << "000" << endl;
                    }
                }
                } //end critical(VERBOSE)
            }
        } //end for
        } //end parallel
        if (verbose) verboseOutput() << endl;

        // delete reducible candidates
        c=Candidates_with_Scalar_Product.begin();
        while(c != Candidates_with_Scalar_Product.end()) {
            if((*c)[0]==-1) {
                c=Candidates_with_Scalar_Product.erase(c);
            } else {
                ++c;
            }
        }
        HBtmp.clear();
    }

    if (verbose) {
        verboseOutput()<<Hilbert_Basis.size()<< " Hilbert Basis elements"<<endl;
    }
}


//---------------------------------------------------------------------------

template<typename Integer>
vector<Integer> Full_Cone<Integer>::compute_degree_function() const {
    if(verbose) {
        verboseOutput()<<"computing degree function... "<<flush;
    }
    size_t i;  
    vector<Integer> degree_function(dim,0);
    if(isComputed(ConeProperty::LinearForm)){ //use Linear_From if we have one
        for (i=0; i<dim; i++) {
            degree_function[i] = Linear_Form[i];
        }
        if(verbose) {
            verboseOutput()<<"using given or homogenous linear form."<<endl;
        }
    } else { // add hyperplanes to get a degree function
        typename list< vector<Integer> >::const_iterator h;
        for (h=Support_Hyperplanes.begin(); h!=Support_Hyperplanes.end(); ++h) {
            for (i=0; i<dim; i++) {
                degree_function[i]+=(*h)[i];
            }
        } 
        v_make_prime(degree_function); //TODO maybe not needed
        if(verbose) {
            verboseOutput()<<"done."<<endl;
        }
    }
    return degree_function;
}

//---------------------------------------------------------------------------

template<typename Integer>
Integer Full_Cone<Integer>::primary_multiplicity() const{
    size_t i,j,k;
    Integer primary_multiplicity=0;
    vector <size_t> key,new_key(dim-1);
    Matrix<Integer> Projection(nr_gen,dim-1);
    for (i = 0; i < nr_gen; i++) {
        for (j = 0; j < dim-1; j++) {
            Projection.write(i,j,Generators.read(i,j));
        }
    }
    typename list< vector<Integer> >::const_iterator h;
    typename list<SHORTSIMPLEX>::const_iterator t;
    for (h =Support_Hyperplanes.begin(); h != Support_Hyperplanes.end(); ++h){
        if ((*h)[dim-1]!=0) {
            for (t =Triangulation.begin(); t!=Triangulation.end(); ++t){
                key=t->key;
                for (i = 0; i <dim; i++) {
                    k=0;
                    for (j = 0; j < dim; j++) {
                        if (j!=i && Generators.read(key[j],dim-1)==1) {
                            if (v_scalar_product(Generators.read(key[j]),(*h))==0) {
                                k++;
                            }
                        }
                        if (k==dim-1) {
                            for (j = 0; j <i; j++) {
                                new_key[j]=key[j];
                            }
                            for (j = i; j <dim-1; j++) {
                                new_key[j]=key[j+1];
                            }
                            Simplex<Integer> S(new_key,Projection);
                            primary_multiplicity+=S.read_volume();
                        }
                    }

                }

            }
        }

    }
    return primary_multiplicity;
}
//---------------------------------------------------------------------------
// Constructors
//---------------------------------------------------------------------------

template<typename Integer>
Full_Cone<Integer>::Full_Cone(){
    dim=0;
    nr_gen=0;
    hyp_size=0;
}

//---------------------------------------------------------------------------
template<typename Integer>
void Full_Cone<Integer>::reset_tasks(){
    do_triangulation = false;
    do_partial_triangulation = false;
    do_Hilbert_basis = false;
    do_ht1_elements = false;
    keep_triangulation = false;
    do_h_vector=false;
    is_pyramid = false;
    
     nrSimplicialPyr=0;
     totalNrPyr=0;
}

//---------------------------------------------------------------------------

template<typename Integer>
Full_Cone<Integer>::Full_Cone(Matrix<Integer> M){
    dim=M.nr_of_columns();
    if (dim!=M.rank()) {
        error_msg("error: Matrix with rank = number of columns needed in the constructor of the object Full_Cone<Integer>.\nProbable reason: Cone not full dimensional (<=> dual cone not pointed)!");
        throw NormalizException();
    }
    Generators = M;
    nr_gen=Generators.nr_of_rows();
    //make the generators coprime and remove 0 rows
    vector<Integer> gcds = Generators.make_prime();
    vector<size_t> key=v_non_zero_pos(gcds);
    if (key.size() < nr_gen) {
        Generators=Generators.submatrix(key);
        nr_gen=Generators.nr_of_rows();
    }
    multiplicity = 0;
    is_Computed =  bitset<ConeProperty::EnumSize>();  //initialized to false
    is_Computed.set(ConeProperty::Generators);
    pointed = false;
    ht1_extreme_rays = false;
    ht1_generated = false;
    ht1_hilbert_basis = false;
    integrally_closed = false;
    
    reset_tasks();
    
    Extreme_Rays = vector<bool>(nr_gen,false);
    // Support_Hyperplanes = list< vector<Integer> >();
    // Triangulation = list<SHORTSIMPLEX>();
    in_triang = vector<bool> (nr_gen,false);
    Facets = list<FACETDATA>();
    // Hilbert_Basis = list< vector<Integer> >();
    // Candidates = list< vector<Integer> >();
    // Ht1_Elements = list< vector<Integer> >();  
    if(dim>0){            //correction needed to include the 0 cone;
        Hilbert_Polynomial = vector<Integer>(2*dim);
        Hilbert_Series = HilbertSeries();
    } else {
        multiplicity = 1;
        Hilbert_Polynomial = vector<Integer>(2,1);
        Hilbert_Polynomial[0] = 0;
        Hilbert_Series = HilbertSeries();
        Hilbert_Series.add_to_num(0);
        is_Computed.set(ConeProperty::HilbertPolynomial);
        is_Computed.set(ConeProperty::HVector);
        is_Computed.set(ConeProperty::Triangulation);
    }
    pyr_level=0;
    Top_Cone=this;
    Top_Key.resize(nr_gen);
    for(size_t i=0;i<nr_gen;i++)
        Top_Key[i]=i;
    totalNrSimplices=0;
    TriangulationSize=0;
    
    HS.resize(omp_get_max_threads());
    FS.resize(omp_get_max_threads());
    
    Pyramids.resize(1);  // prepare storage for pyramids
    recursion_allowed=true;
}

//---------------------------------------------------------------------------

template<typename Integer>
Full_Cone<Integer>::Full_Cone(const Cone_Dual_Mode<Integer> &C) {

    dim = C.dim;
    Generators = C.get_generators();
    nr_gen = Generators.nr_of_rows();

    multiplicity = 0;
    is_Computed =  bitset<ConeProperty::EnumSize>();  //initialized to false
    is_Computed.set(ConeProperty::Generators);
    pointed = true;
    is_Computed.set(ConeProperty::IsPointed);
    ht1_extreme_rays = false;
    ht1_generated = false;
    ht1_hilbert_basis = false;
    integrally_closed = false;
    
    reset_tasks();
    
    Extreme_Rays = vector<bool>(nr_gen,true); //all generators are extreme rays
    is_Computed.set(ConeProperty::ExtremeRays);
    Matrix<Integer> SH = C.SupportHyperplanes;
    Support_Hyperplanes = list< vector<Integer> >();
    for (size_t i=0; i < SH.nr_of_rows(); i++) {
        Support_Hyperplanes.push_back(SH.read(i));
    }
    is_Computed.set(ConeProperty::SupportHyperplanes);
    // Triangulation = list<SHORTSIMPLEX>();
    in_triang = vector<bool>(nr_gen,false);
    // Facets = list<FACETDATA>();
    Hilbert_Basis = C.Hilbert_Basis;
    is_Computed.set(ConeProperty::HilbertBasis);
    Ht1_Elements = list< vector<Integer> >();
    if(dim>0){            //correction needed to include the 0 cone;
        Hilbert_Series = HilbertSeries();
        Hilbert_Polynomial = vector<Integer>(2*dim);
    } else {
        multiplicity = 1;
        Hilbert_Series = HilbertSeries();
        Hilbert_Series.add_to_num(0);
        Hilbert_Polynomial = vector<Integer>(2,1);
        Hilbert_Polynomial[0] = 0;
        is_Computed.set(ConeProperty::HVector);
        is_Computed.set(ConeProperty::HilbertPolynomial);
    }
    pyr_level=0;
    Top_Cone=this;
    Top_Key.resize(nr_gen);
    for(size_t i=0;i<nr_gen;i++)
        Top_Key[i]=i;
    totalNrSimplices=0;
    TriangulationSize=0;
}
//---------------------------------------------------------------------------

/* constructor for pyramids */
template<typename Integer>
Full_Cone<Integer>::Full_Cone(Full_Cone<Integer>& C, const vector<size_t>& Key) {

    Generators = C.Generators.submatrix(Key);
    dim = Generators.nr_of_columns();
    nr_gen = Generators.nr_of_rows();
    
    Top_Cone=C.Top_Cone; // relate to top cone
    Top_Key.resize(nr_gen);
    for(size_t i=0;i<nr_gen;i++)
        Top_Key[i]=C.Top_Key[Key[i]];
  
    multiplicity = 0;
    is_Computed =  ConeProperties();
    
    Extreme_Rays = vector<bool>(nr_gen,false);
    is_Computed.set(ConeProperty::ExtremeRays, C.isComputed(ConeProperty::ExtremeRays));
    if(isComputed(ConeProperty::ExtremeRays))
        for(size_t i=0;i<nr_gen;i++)
            Extreme_Rays[i]=C.Extreme_Rays[Key[i]];
    /* Support_Hyperplanes = list< vector<Integer> >();
    Triangulation = list<SHORTSIMPLEX>();
    Hilbert_Basis = list< vector<Integer> >();
    Ht1_Elements = list< vector<Integer> >();
    Candidates = list< vector<Integer> >();
    Hilbert_Series = HilbertSeries();*/
    in_triang = vector<bool> (nr_gen,false);
    // Facets = list<FACETDATA>();
    
    Linear_Form=C.Linear_Form;
    is_Computed.set(ConeProperty::LinearForm, C.isComputed(ConeProperty::LinearForm));
    Order_Vector=C.Order_Vector;
    
    do_triangulation=C.do_triangulation;
    do_partial_triangulation=C.do_partial_triangulation;
    do_ht1_elements=C.do_ht1_elements;
    do_h_vector=C.do_h_vector;
    do_Hilbert_basis=C.do_Hilbert_basis;
    keep_triangulation=C.keep_triangulation;
    is_pyramid=true;
    pyr_level=C.pyr_level+1;
    totalNrSimplices=0;
    gen_degrees.resize(nr_gen);
    if(isComputed(ConeProperty::LinearForm)){ // now we copy the degrees
        for (size_t i=0; i<nr_gen; i++) {
            gen_degrees[i] = C.gen_degrees[Key[i]];
        }
    }
    TriangulationSize=0;
    
    recursion_allowed=C.recursion_allowed;
}

//---------------------------------------------------------------------------

template<typename Integer>
bool Full_Cone<Integer>::isComputed(ConeProperty::Enum prop) const{
    return is_Computed.test(prop);
}

//---------------------------------------------------------------------------
// Data access
//---------------------------------------------------------------------------

template<typename Integer>
size_t Full_Cone<Integer>::getDimension()const{
    return dim;
}

//---------------------------------------------------------------------------

template<typename Integer>
size_t Full_Cone<Integer>::getNrGenerators()const{
    return nr_gen;
}

//---------------------------------------------------------------------------

template<typename Integer>
bool Full_Cone<Integer>::isPointed()const{
    return pointed;
}

//---------------------------------------------------------------------------

template<typename Integer>
bool Full_Cone<Integer>::isHt1ExtremeRays() const{
    return ht1_extreme_rays;
}

template<typename Integer>
bool Full_Cone<Integer>::isHt1HilbertBasis() const{
    return ht1_hilbert_basis;
}

template<typename Integer>
bool Full_Cone<Integer>::isIntegrallyClosed() const{
    return integrally_closed;
}

//---------------------------------------------------------------------------

template<typename Integer>
vector<Integer> Full_Cone<Integer>::getLinearForm() const{
    return Linear_Form;
}

//---------------------------------------------------------------------------

template<typename Integer>
Integer Full_Cone<Integer>::getMultiplicity()const{
    return multiplicity;
}

//---------------------------------------------------------------------------

template<typename Integer>
const Matrix<Integer>& Full_Cone<Integer>::getGenerators()const{
    return Generators;
}

//---------------------------------------------------------------------------

template<typename Integer>
vector<bool> Full_Cone<Integer>::getExtremeRays()const{
    return Extreme_Rays;
}

//---------------------------------------------------------------------------

template<typename Integer>
Matrix<Integer> Full_Cone<Integer>::getSupportHyperplanes()const{
    size_t s= Support_Hyperplanes.size();
    Matrix<Integer> M(s,dim);
    size_t i=0;
    typename list< vector<Integer> >::const_iterator l;
    for (l =Support_Hyperplanes.begin(); l != Support_Hyperplanes.end(); l++) {
        M.write(i,(*l));
        i++;
    }
    return M;
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::getTriangulation(list< vector<size_t> >& Triang, list<Integer>& TriangVol) const {
    Triang.clear();
    TriangVol.clear();
    vector<size_t> key(dim);
    typename list<SHORTSIMPLEX>::const_iterator l;
    for (l =Triangulation.begin(); l != Triangulation.end(); l++) {
        key=l->key;
        sort(key.begin(),key.end());
        Triang.push_back(key);
        TriangVol.push_back(l->height);
    }
}

//---------------------------------------------------------------------------

template<typename Integer>
Matrix<Integer> Full_Cone<Integer>::getHilbertBasis()const{
    size_t s= Hilbert_Basis.size();
    Matrix<Integer> M(s,dim);
    size_t i=0;
    typename list< vector<Integer> >::const_iterator l;
    for (l =Hilbert_Basis.begin(); l != Hilbert_Basis.end(); l++) {
        M.write(i,(*l));
        i++;
    }
    return M;
}

//---------------------------------------------------------------------------

template<typename Integer>
Matrix<Integer> Full_Cone<Integer>::getHt1Elements()const{
    size_t s= Ht1_Elements.size();
    Matrix<Integer> M(s,dim);
    size_t i=0;
    typename list< vector<Integer> >::const_iterator l;
    for (l =Ht1_Elements.begin(); l != Ht1_Elements.end(); l++) {
        M.write(i,(*l));
        i++;
    }
    return M;
}

//---------------------------------------------------------------------------

template<typename Integer>
vector<Integer> Full_Cone<Integer>::getHilbertPolynomial() const{
    return Hilbert_Polynomial;
}


template<typename Integer>
void Full_Cone<Integer>::error_msg(string s) const{
    errorOutput() <<"\nFull Cone "<< s<<"\n";
}

//---------------------------------------------------------------------------

template<typename Integer>
void Full_Cone<Integer>::print()const{
    verboseOutput()<<"\ndim="<<dim<<".\n";
    verboseOutput()<<"\nnr_gen="<<nr_gen<<".\n";
    verboseOutput()<<"\nhyp_size="<<hyp_size<<".\n";
    verboseOutput()<<"\nHomogeneous is "<<ht1_generated<<".\n";
    verboseOutput()<<"\nLinear_Form is:\n";
    v_read(Linear_Form);
    verboseOutput()<<"\nMultiplicity is "<<multiplicity<<".\n";
    verboseOutput()<<"\nGenerators are:\n";
    Generators.read();
    verboseOutput()<<"\nExtreme_rays are:\n";
    v_read(Extreme_Rays);
    verboseOutput()<<"\nSupport Hyperplanes are:\n";
    l_read(Support_Hyperplanes);
    verboseOutput()<<"\nTriangulation is:\n";
    l_read(Triangulation);
    verboseOutput()<<"\nHilbert basis is:\n";
    l_read(Hilbert_Basis);
    verboseOutput()<<"\nHt1 elements are:\n";
    l_read(Ht1_Elements);
    verboseOutput()<<"\nHilbert Series  is:\n";
    verboseOutput()<<Hilbert_Series;
    verboseOutput()<<"\nHilbert polynomial is:\n";
    v_read(Hilbert_Polynomial);
}

} //end namespace


