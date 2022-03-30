#pragma once

#include <limits>
#include <vector>
#include <algorithm>

// internal type
struct Alias
{
    float t;    // split
    int i;      // alias
};

struct AliasTable
{
    int n;
    std::vector<Alias> table;

    AliasTable(const std::vector<float>& values) : n(values.size()), table(n)
    {
        const float sum = std::accumulate( values.begin(), values.end(), 0.f );
        const float avg = sum / n;

        std::vector<int> partition(n);
        int lid = 0, hid = n-1;
        for(int i = 0; i < n; ++i)
        {
            table[i].t = values[i]/avg;
            if( table[i].t <= 1 ) 
                partition[lid++] = i;
            else 
                partition[hid--] = i;
        }

        lid = 0, hid = n-1;
        int tlid = partition[lid], thid = partition[hid], nthid; 

        // construct alias table
        while(lid < hid)
        {   
            if( table[thid].t > 1 )
            {
                table[tlid].i = thid;
                table[thid].t -= (1 - table[tlid].t);
                tlid = partition[++lid];
            }
            else
            {
                nthid = partition[--hid];
                table[thid].i = nthid;
                table[nthid].t -= (1 - table[thid].t);
                thid = nthid;
            }
        }
        // last item should have 100% chance to be picked 
        table[thid] = {1, std::numeric_limits<int>::max()}; 
    }

    int sample(const float u)
    {
        const int id = n * u;        // uniform [0, n)
        assert(id < n);
        const float u2 = n * u - id; // uniform [0, 1)
        return sample(u, u2);
    }
};

struct DiscreteCDF
{
    // data
    int n;
    float total;
    std::vector<float> cdf;

    DiscreteCDF(const std::vector<float>& values) : n(values.size()), total(0), cdf(n)
    {
        // build cdf
        for(int i = 0; i < n; ++i)
        {
            total += values[i];
            cdf[i] = total;
        }
        // printf("total %f\n", total);
    }

    // Performs a binary search in an ordered array (cdf) of size (size).
    // The searched value must be lower than the greater element (cdf[size-1]).
    static int binary(const float value, const float* cdf, const int size)
    {
        assert( value <= cdf[size-1] );
        int p = 0, q = size - 1;
        while(p < q)
        {
            int m = (p+q) / 2;
            if(cdf[m] < value)
               p = m + 1;
            else
               q = m;
        }
        assert(p >= 0 && p < size);
        return p;
    }

    int sample(const float u)
    {
        return binary(u * total, cdf.data(), n);
    }
};
