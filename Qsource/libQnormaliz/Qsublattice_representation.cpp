/*
 * Normaliz
 * Copyright (C) 2007-2014  Winfried Bruns, Bogdan Ichim, Christof Soeger
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
 * As an exception, when this program is distributed through (i) the App Store
 * by Apple Inc.; (ii) the Mac App Store by Apple Inc.; or (iii) Google Play
 * by Google Inc., then that store may impose any digital rights management,
 * device limits and/or redistribution restrictions that are required by its
 * terms of service.
 */

/**
 * The class Sublattice_Representation represents a sublattice of Z^n as Z^r.
 * To transform vectors of the sublattice  use:
 *    Z^r --> Z^n    and    Z^n -->  Z^r
 *     v  |-> vA             u  |-> (uB)/c
 * A  r x n matrix
 * B  n x r matrix
 * c  Number
 * 
 * We have kept c though it is always 1 for coefficients over a field
 */


#include "libQnormaliz/Qsublattice_representation.h"
#include "libQnormaliz/Qvector_operations.h"

//---------------------------------------------------------------------------

namespace libQnormaliz {
using namespace std;

/**
 * creates a representation of Z^n as a sublattice of itself
 */
template<typename Number>
Sublattice_Representation<Number>::Sublattice_Representation(size_t n) {
    dim = n;
    rank = n;
    external_index = 1;
    A = Matrix<Number>(n);
    B = Matrix<Number>(n);
    c = 1;
    Equations_computed=false;
    is_identity=true;
}

//---------------------------------------------------------------------------

/**
 * Main Constructor
 * creates a representation of a sublattice of Z^n
 * if direct_summand is false the sublattice is generated by the rows of M
 * otherwise it is a direct summand of Z^n containing the rows of M
 */
 
 template<typename Number>
Sublattice_Representation<Number>::Sublattice_Representation(const Matrix<Number>& M, bool take_saturation) {
    initialize(M); // take saturation is complewtely irrelevant for coefficients in a field
}


template<typename Number>
void Sublattice_Representation<Number>::initialize(const Matrix<Number>& M) {

    Equations_computed=false;
    is_identity=false;

    dim=M.nr_of_columns();
    Matrix<Number> N=M;    

    bool success; // dummy for field coefficients
    rank=N.row_echelon_reduce(success); // cleans corner columns and makes corner elements positive

    if(rank==dim){
        A = B = Matrix<Number>(dim);
        c=1;
        is_identity=true;
        return;   
    }

    vector<key_t> col(rank);
    vector<bool> col_is_corner(dim,false);
    for(size_t k=0;k<rank;++k){
        size_t j=0;
        for(;j<dim;++j)
            if(N[k][j]!=0)
                break;
        col_is_corner[j]=true;
        col[k]=j;
        if(N[k][j]<0)
            v_scalar_multiplication<Number>(N[k],-1);
    }
    
    A=Matrix<Number>(rank, dim);
    B=Matrix<Number>(dim,rank);
    
    for(size_t k=0;k<rank;++k)
        A[k]=N[k];
    size_t j=0;
    for(size_t k=0;k<dim;++k){
        if(col_is_corner[k]){
            B[k][j]=1/A[j][k]; //to make the inverse of the diagonal matrix that we get 
            j++;               // by extracting the corner columns
        }
    };
    c=1;
    return;               

}

//---------------------------------------------------------------------------
//                       Constructor by conversion
//---------------------------------------------------------------------------

template<typename Number>
template<typename NumberFC>
Sublattice_Representation<Number>::Sublattice_Representation(const 
             Sublattice_Representation<NumberFC>& Original) {
                 
    convert(A,Original.A);
    convert(B,Original.B);
    dim=Original.dim;
    rank=Original.rank;
    convert(c,Original.c);
    is_identity=Original.is_identity;
    Equations_computed=Original.Equations_computed;
    convert(Equations,Original.Equations);
    external_index=Original.external_index;    
}


//---------------------------------------------------------------------------
//                       Manipulation operations
//---------------------------------------------------------------------------

/* first this then SR when going from Z^n to Z^r */
template<typename Number>
void Sublattice_Representation<Number>::compose(const Sublattice_Representation& SR) {
    assert(rank == SR.dim); //TODO vielleicht doch exception?
    
    if(SR.is_identity)
        return;
    
    if(is_identity){
        *this=SR;
        return;
    }        
    
    Equations_computed=false;


    rank = SR.rank;
    // A = SR.A * A
    A = SR.A.multiplication(A);
    // B = B * SR.B
    B = B.multiplication(SR.B);
    c = c * SR.c;
    
    is_identity&=SR.is_identity;
}

template<typename Number>
void Sublattice_Representation<Number>::compose_dual(const Sublattice_Representation& SR) {

    assert(rank == SR.dim); //
    assert(SR.c==1);
    
    if(SR.is_identity)
        return;
    
    Equations_computed=false;
    rank = SR.rank;
    
    if(is_identity){
        A=SR.B.transpose();
        B=SR.A.transpose();
        is_identity=false;
        return;
    }
    
    // Now we compose with the dual of SR
    A = SR.B.transpose().multiplication(A);
    // B = B * SR.B
    B = B.multiplication(SR.A.transpose());
    
    //check if a factor can be extraced from B  //TODO necessary?
    Number g=1; // = B.matrix_gcd();
    is_identity&=SR.is_identity;
}

//---------------------------------------------------------------------------
//                       Transformations
//---------------------------------------------------------------------------

template<typename Number>
Matrix<Number> Sublattice_Representation<Number>::to_sublattice (const Matrix<Number>& M) const {
    Matrix<Number> N;
    if(is_identity)
        N=M;
    else        
        N = M.multiplication(B);
    if (c!=1) N.scalar_division(c);
    return N;
}
template<typename Number>
Matrix<Number> Sublattice_Representation<Number>::from_sublattice (const Matrix<Number>& M) const {
    Matrix<Number> N;
    if(is_identity)
        N=M;
    else        
        N = M.multiplication(A);
    return N;
}

template<typename Number>
Matrix<Number> Sublattice_Representation<Number>::to_sublattice_dual (const Matrix<Number>& M) const {
    Matrix<Number> N;
    if(is_identity)
        N=M;
    else        
        N = M.multiplication(A.transpose());
    N.simplify_rows();
    return N;
}

template<typename Number>
Matrix<Number> Sublattice_Representation<Number>::from_sublattice_dual (const Matrix<Number>& M) const {
    Matrix<Number> N;
    if(is_identity)
        N=M;
    else        
        N =  M.multiplication(B.transpose());
    N.simplify_rows();
    return N;
}


template<typename Number>
vector<Number> Sublattice_Representation<Number>::to_sublattice (const vector<Number>& V) const {
    if(is_identity)
        return V;
    vector<Number> N = B.VxM(V);
    if (c!=1) v_scalar_division(N,c);
    return N;
}

template<typename Number>
vector<Number> Sublattice_Representation<Number>::from_sublattice (const vector<Number>& V) const {
    if(is_identity)
        return V;
    vector<Number> N = A.VxM(V);
    return N;
}

template<typename Number>
vector<Number> Sublattice_Representation<Number>::to_sublattice_dual (const vector<Number>& V) const {
    vector<Number> N;
    if(is_identity)
        N=V;
    else    
        N = A.MxV(V);
    v_simplify(N);
    return N;
}

template<typename Number>
vector<Number> Sublattice_Representation<Number>::from_sublattice_dual (const vector<Number>& V) const {
    vector<Number> N; 
    if(is_identity)
        N=V;
    else    
        N = B.MxV(V);
    v_simplify(N);
    return N;
}

template<typename Number>
vector<Number> Sublattice_Representation<Number>::to_sublattice_dual_no_div (const vector<Number>& V) const {
    if(is_identity)
        return V;
    vector<Number> N = A.MxV(V);
    return N;
}

//---------------------------------------------------------------------------
//                       Data access
//---------------------------------------------------------------------------

/* returns the dimension of the ambient space */
template<typename Number>
size_t Sublattice_Representation<Number>::getDim() const {
    return dim;
}

//---------------------------------------------------------------------------

/* returns the rank of the sublattice */
template<typename Number>
size_t Sublattice_Representation<Number>::getRank() const {
    return rank;
}

//---------------------------------------------------------------------------

template<typename Number>
const Matrix<Number>& Sublattice_Representation<Number>::getEmbeddingMatrix() const {
    return A;
} 

template<typename Number>
const vector<vector<Number> >& Sublattice_Representation<Number>::getEmbedding() const{
    return getEmbeddingMatrix().get_elements();
}

//---------------------------------------------------------------------------

template<typename Number>
const Matrix<Number>& Sublattice_Representation<Number>::getProjectionMatrix() const {
    return B;
}

template<typename Number>
const vector<vector<Number> >& Sublattice_Representation<Number>::getProjection() const{
    return getProjectionMatrix().get_elements();
}


//---------------------------------------------------------------------------

template<typename Number>
Number Sublattice_Representation<Number>::getAnnihilator() const {
    return c;
}

//---------------------------------------------------------------------------

template<typename Number>
bool Sublattice_Representation<Number>::IsIdentity() const{ 
    return is_identity;
}

//---------------------------------------------------------------------------


template<typename Number>
const Matrix<Number>& Sublattice_Representation<Number>::getEquationsMatrix() const{

    if(!Equations_computed)
        make_equations();
    return Equations;
}

template<typename Number>
const vector<vector<Number> >& Sublattice_Representation<Number>::getEquations() const{
        return getEquationsMatrix().get_elements();
}

template<typename Number>
void Sublattice_Representation<Number>::make_equations() const{

    if(rank==dim)
        Equations=Matrix<Number>(0,dim);
    else
        Equations=A.kernel(); 
        Equations.simplify_rows();
    Equations_computed=true;
}


}
