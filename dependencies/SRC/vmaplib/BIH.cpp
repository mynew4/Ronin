/***
 * Demonstrike Core
 */

#include <g3dlite\G3D.h>
#include "VMapLib.h"
#include "VMapDefinitions.h"

void BIH::buildHierarchy(std::vector<G3D::uint32> &tempTree, buildData &dat, BuildStats &stats)
{
    // create space for the first node
    tempTree.push_back(G3D::uint32(3 << 30)); // dummy leaf
    tempTree.insert(tempTree.end(), 2, 0);
    //tempTree.add(0);

    // seed bbox
    AABound gridBox = { bounds.low(), bounds.high() };
    AABound nodeBox = gridBox;
    // seed subdivide function
    subdivide(0, dat.numPrims - 1, tempTree, dat, gridBox, nodeBox, 0, 1, stats);
}

void BIH::subdivide(int left, int right, std::vector<G3D::uint32> &tempTree, buildData &dat, AABound &gridBox, AABound &nodeBox, int nodeIndex, int depth, BuildStats &stats)
{
    if ((right - left + 1) <= dat.maxPrims || depth >= MAX_STACK_SIZE)
    {
        // write leaf node
        stats.updateLeaf(depth, right - left + 1);
        createNode(tempTree, nodeIndex, left, right);
        return;
    }
    // calculate extents
    int axis = -1, prevAxis, rightOrig;
    float clipL = G3D::fnan(), clipR = G3D::fnan(), prevClip = G3D::fnan();
    float split = G3D::fnan(), prevSplit;
    bool wasLeft = true;
    while (true)
    {
        prevAxis = axis;
        prevSplit = split;
        // perform quick consistency checks
        G3D::Vector3 d( gridBox.hi - gridBox.lo );
        if (d.x < 0 || d.y < 0 || d.z < 0)
            throw std::logic_error("negative node extents");
        for (int i = 0; i < 3; i++)
        {
            if (nodeBox.hi[i] < gridBox.lo[i] || nodeBox.lo[i] > gridBox.hi[i])
            {
                //UI.printError(Module.ACCEL, "Reached tree area in error - discarding node with: %d objects", right - left + 1);
                throw std::logic_error("invalid node overlap");
            }
        }
        // find longest axis
        axis = d.primaryAxis();
        split = 0.5f * (gridBox.lo[axis] + gridBox.hi[axis]);
        // partition L/R subsets
        clipL = -G3D::inf();
        clipR = G3D::inf();
        rightOrig = right; // save this for later
        float nodeL = G3D::inf();
        float nodeR = -G3D::inf();
        for (int i = left; i <= right;)
        {
            int obj = dat.indices[i];
            float minb = dat.primBound[obj].low()[axis];
            float maxb = dat.primBound[obj].high()[axis];
            float center = (minb + maxb) * 0.5f;
            if (center <= split)
            {
                // stay left
                i++;
                if (clipL < maxb)
                    clipL = maxb;
            }
            else
            {
                // move to the right most
                int t = dat.indices[i];
                dat.indices[i] = dat.indices[right];
                dat.indices[right] = t;
                right--;
                if (clipR > minb)
                    clipR = minb;
            }
            nodeL = std::min(nodeL, minb);
            nodeR = std::max(nodeR, maxb);
        }
        // check for empty space
        if (nodeL > nodeBox.lo[axis] && nodeR < nodeBox.hi[axis])
        {
            float nodeBoxW = nodeBox.hi[axis] - nodeBox.lo[axis];
            float nodeNewW = nodeR - nodeL;
            // node box is too big compare to space occupied by primitives?
            if (1.3f * nodeNewW < nodeBoxW)
            {
                stats.updateBVH2();
                int nextIndex = (int)tempTree.size();
                // allocate child
                tempTree.push_back(0);
                tempTree.push_back(0);
                tempTree.push_back(0);
                // write bvh2 clip node
                stats.updateInner();
                tempTree[nodeIndex + 0] = (axis << 30) | (1 << 29) | nextIndex;
                tempTree[nodeIndex + 1] = floatToRawIntBits(nodeL);
                tempTree[nodeIndex + 2] = floatToRawIntBits(nodeR);
                // update nodebox and recurse
                nodeBox.lo[axis] = nodeL;
                nodeBox.hi[axis] = nodeR;
                subdivide(left, rightOrig, tempTree, dat, gridBox, nodeBox, nextIndex, depth + 1, stats);
                return;
            }
        }
        // ensure we are making progress in the subdivision
        if (right == rightOrig)
        {
            // all left
            if (prevAxis == axis && G3D::fuzzyEq(prevSplit, split)) {
                // we are stuck here - create a leaf
                stats.updateLeaf(depth, right - left + 1);
                createNode(tempTree, nodeIndex, left, right);
                return;
            }
            if (clipL <= split) {
                // keep looping on left half
                gridBox.hi[axis] = split;
                prevClip = clipL;
                wasLeft = true;
                continue;
            }
            gridBox.hi[axis] = split;
            prevClip = G3D::fnan();
        }
        else if (left > right)
        {
            // all right
            if (prevAxis == axis && G3D::fuzzyEq(prevSplit, split)) {
                // we are stuck here - create a leaf
                stats.updateLeaf(depth, right - left + 1);
                createNode(tempTree, nodeIndex, left, right);
                return;
            }
            right = rightOrig;
            if (clipR >= split) {
                // keep looping on right half
                gridBox.lo[axis] = split;
                prevClip = clipR;
                wasLeft = false;
                continue;
            }
            gridBox.lo[axis] = split;
            prevClip = G3D::fnan();
        }
        else
        {
            // we are actually splitting stuff
            if (prevAxis != -1 && !G3D::isNaN(prevClip))
            {
                // second time through - lets create the previous split
                // since it produced empty space
                int nextIndex = (int)tempTree.size();
                // allocate child node
                tempTree.push_back(0);
                tempTree.push_back(0);
                tempTree.push_back(0);
                if (wasLeft) {
                    // create a node with a left child
                    // write leaf node
                    stats.updateInner();
                    tempTree[nodeIndex + 0] = (prevAxis << 30) | nextIndex;
                    tempTree[nodeIndex + 1] = floatToRawIntBits(prevClip);
                    tempTree[nodeIndex + 2] = floatToRawIntBits(G3D::inf());
                } else {
                    // create a node with a right child
                    // write leaf node
                    stats.updateInner();
                    tempTree[nodeIndex + 0] = (prevAxis << 30) | (nextIndex - 3);
                    tempTree[nodeIndex + 1] = floatToRawIntBits(-G3D::inf());
                    tempTree[nodeIndex + 2] = floatToRawIntBits(prevClip);
                }
                // count stats for the unused leaf
                depth++;
                stats.updateLeaf(depth, 0);
                // now we keep going as we are, with a new nodeIndex:
                nodeIndex = nextIndex;
            }
            break;
        }
    }
    // compute index of child nodes
    int nextIndex = (int)tempTree.size();
    // allocate left node
    int nl = right - left + 1;
    int nr = rightOrig - (right + 1) + 1;
    if (nl > 0) {
        tempTree.push_back(0);
        tempTree.push_back(0);
        tempTree.push_back(0);
    } else
        nextIndex -= 3;
    // allocate right node
    if (nr > 0) {
        tempTree.push_back(0);
        tempTree.push_back(0);
        tempTree.push_back(0);
    }
    // write leaf node
    stats.updateInner();
    tempTree[nodeIndex + 0] = (axis << 30) | nextIndex;
    tempTree[nodeIndex + 1] = floatToRawIntBits(clipL);
    tempTree[nodeIndex + 2] = floatToRawIntBits(clipR);
    // prepare L/R child boxes
    AABound gridBoxL(gridBox), gridBoxR(gridBox);
    AABound nodeBoxL(nodeBox), nodeBoxR(nodeBox);
    gridBoxL.hi[axis] = gridBoxR.lo[axis] = split;
    nodeBoxL.hi[axis] = clipL;
    nodeBoxR.lo[axis] = clipR;
    // recurse
    if (nl > 0)
        subdivide(left, right, tempTree, dat, gridBoxL, nodeBoxL, nextIndex, depth + 1, stats);
    else
        stats.updateLeaf(depth + 1, 0);
    if (nr > 0)
        subdivide(right + 1, rightOrig, tempTree, dat, gridBoxR, nodeBoxR, nextIndex + 3, depth + 1, stats);
    else
        stats.updateLeaf(depth + 1, 0);
}

bool BIH::writeToFile(FILE *wf) const
{
    size_t check = 0;
    G3D::uint32 treeSize = (G3D::uint32)tree.size(), count=(G3D::uint32)objects.size();
    check += fwrite(&bounds.low(), sizeof(float), 3, wf);
    check += fwrite(&bounds.high(), sizeof(float), 3, wf);
    check += fwrite(&treeSize, sizeof(G3D::uint32), 1, wf);
    check += fwrite(&count, sizeof(G3D::uint32), 1, wf);
    if(treeSize)
    {
        for(uint32 i = 0; i < treeSize; i++)
            check += fwrite(&tree[i], sizeof(G3D::uint32), 1, wf);
    }

    if(count)
    {
        for(uint32 i = 0; i < count; i++)
            check += fwrite(&objects[i], sizeof(G3D::uint32), 1, wf);
    }
    return check == (3 + 3 + 2 + treeSize + count);
}

bool BIH::readFromFile(FILE *rf)
{
    G3D::Vector3 lo, hi;
    G3D::uint32 treeSize = 0, count=0;
    size_t check = 0;
    if((check = (fread(&lo, sizeof(float), 3, rf) + fread(&hi, sizeof(float), 3, rf))) != 6)
        return false;
    if((check = fread(&treeSize, sizeof(G3D::uint32), 1, rf)) != 1)
        return false;
    if((check = fread(&count, sizeof(G3D::uint32), 1, rf)) != 1)
        return false;

    bounds = G3D::AABox(lo, hi);
    if(treeSize)
    {
        tree.resize(treeSize);
        for(uint32 i = 0; i < treeSize; i++)
        {
            if(fread(&tree[i], sizeof(G3D::uint32), 1, rf) != 1)
                return false;
        }
    }

    if(count)
    {
        objects.resize(count);
        for(uint32 i = 0; i < count; i++)
        {
            if(fread(&objects[i], sizeof(G3D::uint32), 1, rf) != 1)
                return false;
        }
    }
    return true;
}

void BIH::BuildStats::updateLeaf(int depth, int n)
{
    numLeaves++;
    minDepth = std::min(depth, minDepth);
    maxDepth = std::max(depth, maxDepth);
    sumDepth += depth;
    minObjects = std::min(n, minObjects);
    maxObjects = std::max(n, maxObjects);
    sumObjects += n;
    int nl = std::min(n, 5);
    ++numLeavesN[nl];
}

void BIH::BuildStats::printStats()
{
    OUT_DEBUG("Tree stats:");
    OUT_DEBUG("  * Nodes:          %d", numNodes);
    OUT_DEBUG("  * Leaves:         %d", numLeaves);
    OUT_DEBUG("  * Objects: min    %d", minObjects);
    OUT_DEBUG("             avg    %.2f", (float) sumObjects / numLeaves);
    OUT_DEBUG("           avg(n>0) %.2f", (float) sumObjects / (numLeaves - numLeavesN[0]));
    OUT_DEBUG("             max    %d", maxObjects);
    OUT_DEBUG("  * Depth:   min    %d", minDepth);
    OUT_DEBUG("             avg    %.2f", (float) sumDepth / numLeaves);
    OUT_DEBUG("             max    %d", maxDepth);
    OUT_DEBUG("  * Leaves w/: N=0  %3d%%", 100 * numLeavesN[0] / numLeaves);
    OUT_DEBUG("               N=1  %3d%%", 100 * numLeavesN[1] / numLeaves);
    OUT_DEBUG("               N=2  %3d%%", 100 * numLeavesN[2] / numLeaves);
    OUT_DEBUG("               N=3  %3d%%", 100 * numLeavesN[3] / numLeaves);
    OUT_DEBUG("               N=4  %3d%%", 100 * numLeavesN[4] / numLeaves);
    OUT_DEBUG("               N>4  %3d%%", 100 * numLeavesN[5] / numLeaves);
    OUT_DEBUG("  * BVH2 nodes:     %d (%3d%%)", numBVH2, 100 * numBVH2 / (numNodes + numLeaves - 2 * numBVH2));
}
