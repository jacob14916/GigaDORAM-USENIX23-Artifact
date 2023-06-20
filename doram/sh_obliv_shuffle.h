#pragma once

#include "local_permutation.h"
#include "rep_share.h"
#include "utils.h"

namespace emp
{

///* suggestion: have a reshare 3->2 (make that malicously secure) then the 2 parties shuffle, then reshare 2->3 (make
/// that malicously secure) {reshare2->3 DOES NEED MACS}
// both of those things will be easier, require no MACs
template <typename T, typename S> // ! mallocs local perm for shufflers only
void sh_known_to_2players_shuffle(
    int shuffler1, int shuffler2, u_int list_len, rep_share<T> *share_list, LocalPermutation *&loc_perm,
    rep_share<S> *share_list2 = nullptr) //*& because we want to change where the pointer points and simplicity
{
    auto _start = clock_start();

    assert(shuffler1 != shuffler2);
    T *nonrep_share_list = new T[list_len];
    S *nonrep_share_list2;
    if (share_list2 != nullptr)
    {
        nonrep_share_list2 = new S[list_len];
    }

    reshare_3to2(shuffler1, shuffler2, share_list, list_len, nonrep_share_list);
    if (share_list2 != nullptr)
    {
        reshare_3to2(shuffler1, shuffler2, share_list2, list_len, nonrep_share_list2);
    }
    if (party == shuffler1)
    {
        loc_perm = new LocalPermutation(prgs[shuffler2], list_len);
        loc_perm->shuffle(nonrep_share_list);
        if (share_list2 != nullptr)
        {
            loc_perm->shuffle(nonrep_share_list2);
        }
    }
    else if (party == shuffler2)
    {
        loc_perm = new LocalPermutation(prgs[shuffler1], list_len);
        loc_perm->shuffle(nonrep_share_list);
        if (share_list2 != nullptr)
        {
            loc_perm->shuffle(nonrep_share_list2);
        }
    }
    reshare_2to3(shuffler1, shuffler2, nonrep_share_list, list_len, share_list);
    if (share_list2 != nullptr)
    {
        reshare_2to3(shuffler1, shuffler2, nonrep_share_list2, list_len, share_list2);
    }

    delete[] nonrep_share_list;
    delete[] nonrep_share_list2;

    time_total_shuffles += time_from(_start);
}

template <typename T>
void _sh_obliv_shuffle_forward(rep_share<T> *rep_shares, uint list_len, LocalPermutation &prev_shared_perm,
                               LocalPermutation &next_shared_perm)
{
    T *two_shares = new T[list_len];
    for (int p = 1; p <= 3; p++)
    {
        reshare_3to2(p, next_party(p), rep_shares, list_len, two_shares);
        if (party == p)
        {
            next_shared_perm.shuffle(two_shares);
        }
        else if (party == next_party(p))
        {
            prev_shared_perm.shuffle(two_shares);
        }
        reshare_2to3(p, next_party(p), two_shares, list_len, rep_shares);
    }
    delete[] two_shares;
}

template <typename T>
void _sh_obliv_shuffle_inverse(rep_share<T> *rep_shares, uint list_len, LocalPermutation &prev_shared_perm,
                               LocalPermutation &next_shared_perm)
{
    T *two_shares = new T[list_len];
    for (int p = 3; p >= 1; p--)
    {
        reshare_3to2(p, next_party(p), rep_shares, list_len, two_shares);
        if (party == p)
        {
            next_shared_perm.inverse_shuffle(two_shares);
        }
        else if (party == next_party(p))
        {
            prev_shared_perm.inverse_shuffle(two_shares);
        }
        reshare_2to3(p, next_party(p), two_shares, list_len, rep_shares);
    }
    delete[] two_shares;
}

// the base case of the type recursion
void _sh_obliv_shuffle_multiple(uint list_len, LocalPermutation &prev_shared_perm, LocalPermutation &next_shared_perm)
{
}

template <typename Head, typename... Tail>
void _sh_obliv_shuffle_multiple(uint list_len, LocalPermutation &prev_shared_perm, LocalPermutation &next_shared_perm,
                                rep_share<Head> *rep_shares, Tail... T)
{
    auto _start = clock_start();
    _sh_obliv_shuffle_forward(rep_shares, list_len, prev_shared_perm, next_shared_perm);
    _sh_obliv_shuffle_multiple(list_len, prev_shared_perm, next_shared_perm, T...);
    time_total_shuffles += time_from(_start);
}

template <typename... T> void sh_obliv_shuffle(u_int list_len, rep_share<uint> *perm_eval, T... rep_share_lists)
{ //! do rep_share_lists need to be of the same type?
    LocalPermutation prev_shared_perm(prev_prg, list_len), next_shared_perm(next_prg, list_len);
    _sh_obliv_shuffle_multiple(list_len, prev_shared_perm, next_shared_perm, rep_share_lists...);
    if (perm_eval)
    {
        for (uint i = 0; i < list_len; i++)
        {
            input_public_to_replicated(i, perm_eval + i);
        }
        _sh_obliv_shuffle_inverse(perm_eval, list_len, prev_shared_perm, next_shared_perm);
    }
}
} // namespace emp