/*
 * Copyright (C) 2004-2009 Andrew Mihal
 * Copyright (C) 2009-2016 Christoph Spiel
 *
 * This file is part of Enblend.
 *
 * Enblend is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Enblend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Enblend; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef PATH_H_INCLUDED_
#define PATH_H_INCLUDED_


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <array>
#include <iostream>
#include <queue>
#include <vector>


namespace enblend
{
    template <typename Point, typename Image>
    class PathCompareFunctor : public std::binary_function<Point, Point, bool>
    {
    public:
        explicit PathCompareFunctor(const Image* an_image) : image_(an_image) {}

        bool operator()(const Point& a_point, const Point& another_point) const {
            if (parameter::as_boolean("debug-path-compare", false)) {
                std::cout << "+ PathCompareFunctor::operator(): comparing "
                          << "cost(p1 = " << a_point << ") = " << (*image_)[a_point] << " and "
                          << "cost(p2 = " << another_point << ") = " << (*image_)[another_point]
                          << std::endl;
            }
            // want the priority queue sorted in ascending order
            return (*image_)[a_point] > (*image_)[another_point];
        }

    private:
        const Image* const image_;
    }; // class PathCompareFunctor


    template <class CostImageIterator, class CostAccessor>
    std::vector<vigra::Point2D>*
    minCostPath(CostImageIterator cost_upperleft, CostImageIterator cost_lowerright, CostAccessor cost_accessor,
                const vigra::Point2D& startingPoint, const vigra::Point2D& endingPoint)
    {
        typedef typename CostAccessor::value_type CostPixelType;
        typedef typename vigra::NumericTraits<CostPixelType>::Promote WorkingPixelType;
        typedef vigra::BasicImage<WorkingPixelType> WorkingImageType;
        typedef std::priority_queue<vigra::Point2D, std::vector<vigra::Point2D>,
                                    PathCompareFunctor<vigra::Point2D, WorkingImageType> > PriorityQueue;

        // 4-bit direction encoding {up, down, left, right}
        // A  8  9
        // 2  0  1
        // 6  4  5
        //
        // ANTICIPATED CHANGE: Check whether it is faster to pull out
        // `neighborArray' and `neighborArrayInverse' as `static
        // const'.
        const std::array<vigra::UInt8, 8> neighborArray = {0xA, 1, 6, 8, 5, 2, 9, 4};
        const std::array<vigra::UInt8, 8> neighborArrayInverse = {5, 2, 9, 4, 0xA, 1, 6, 8};

        const vigra::Size2D size(cost_lowerright - cost_upperleft);
        const vigra::Rect2D valid_region(size);

        vigra::UInt8Image pathNextHop(size);
        WorkingImageType costSoFar(size, vigra::NumericTraits<WorkingPixelType>::max());
        // The doubled parentheses in the definition of `pq' avoid "C++'s most-vexing parse", gosh!
        PriorityQueue pq((PathCompareFunctor<vigra::Point2D, WorkingImageType>(&costSoFar)));
        std::vector<vigra::Point2D>* result = new std::vector<vigra::Point2D>;

        if (parameter::as_boolean("debug-path", false)) {
            std::cout << "+ minCostPath: size = " << size << "\n"
                      << "+ minCostPath: startingPoint = " << startingPoint
                      << (valid_region.contains(startingPoint) ? "" : " (invalid)")
                      << ", endingPoint = " << endingPoint
                      << (valid_region.contains(endingPoint) ? "" : " (invalid)")
                      << std::endl;
        }

        if (valid_region.contains(endingPoint)) {
            costSoFar[endingPoint] =
                std::max(vigra::NumericTraits<WorkingPixelType>::one(),
                         vigra::NumericTraits<WorkingPixelType>::toPromote(cost_accessor(cost_upperleft + endingPoint)));
            pq.push(endingPoint);
        }

        while (!pq.empty()) {
            vigra::Point2D top = pq.top();
            pq.pop();
            if (parameter::as_boolean("debug-path", false)) {
                std::cout << "+ minCostPath: visiting point = " << top << std::endl;
            }

            if (top != startingPoint) {
                WorkingPixelType costToTop = costSoFar[top];
                if (parameter::as_boolean("debug-path", false)) {
                    std::cout << "+ minCostPath: costToTop = " << costToTop << std::endl;
                }

                // For each 8-neighbor of top with costSoFar == 0 do relax
                for (int i = 0; i < 8; i++) {
                    // Get the neighbor
                    vigra::UInt8 neighborDirection = neighborArray[i];
                    vigra::Point2D neighborPoint = top;
                    if (neighborDirection & 0x8) {--neighborPoint.y;}
                    if (neighborDirection & 0x4) {++neighborPoint.y;}
                    if (neighborDirection & 0x2) {--neighborPoint.x;}
                    if (neighborDirection & 0x1) {++neighborPoint.x;}

                    // Make sure neighbor is in valid region
                    if (!valid_region.contains(neighborPoint)) {
                        continue;
                    }
                    if (parameter::as_boolean("debug-path", false)) {
                        std::cout << "+ minCostPath: neighbor = " << neighborPoint << std::endl;
                    }

                    // See if the neighbor has already been visited.
                    // If neighbor has maximal cost, it has not been visited.
                    // If so skip it.
                    WorkingPixelType neighborPreviousCost = costSoFar[neighborPoint];
                    if (parameter::as_boolean("debug-path", false)) {
                        std::cout <<
                            "+ minCostPath: neighborPreviousCost = " << neighborPreviousCost << std::endl;
                    }
                    if (neighborPreviousCost != vigra::NumericTraits<WorkingPixelType>::max()) {
                        continue;
                    }

                    WorkingPixelType neighborCost =
                        std::max(vigra::NumericTraits<WorkingPixelType>::one(),
                                 vigra::NumericTraits<WorkingPixelType>::toPromote(cost_accessor(cost_upperleft + neighborPoint)));
                    if (parameter::as_boolean("debug-path", false)) {
                        std::cout << "+ minCostPath: neighborCost = " << neighborCost << std::endl;
                    }
                    if (neighborCost == vigra::NumericTraits<CostPixelType>::max()) {
                        neighborCost *= 65536; // Can't use << since neighborCost may be floating-point
                    }

                    if ((i & 1) == 0) {
                        // neighbor is diagonal
                        neighborCost = WorkingPixelType(static_cast<double>(neighborCost) * 1.4);
                    }

                    const WorkingPixelType newNeighborCost
                    {vigra::NumericTraits<WorkingPixelType>::fromRealPromote(static_cast<double>(neighborCost) +
                                                                             static_cast<double>(costToTop))};

                    if (newNeighborCost < neighborPreviousCost) {
                        // We have found the shortest path to neighbor.
                        costSoFar[neighborPoint] = newNeighborCost;
                        pathNextHop[neighborPoint] = neighborArrayInverse[i];
                        pq.push(neighborPoint);
                    }
                }
            } else {
                // If yes then follow back to beginning using pathNextHop
                // include neither start nor end point in result
                vigra::UInt8 nextHop = pathNextHop[top];
                while (nextHop != 0) {
                    if (nextHop & 0x8) {--top.y;}
                    if (nextHop & 0x4) {++top.y;}
                    if (nextHop & 0x2) {--top.x;}
                    if (nextHop & 0x1) {++top.x;}
                    nextHop = pathNextHop[top];
                    if (nextHop != 0) {
                        result->push_back(top);
                    }
                }
                break;
            }
        }

        return result;
    }


    template <class CostImageIterator, class CostAccessor>
    inline static std::vector<vigra::Point2D>*
    minCostPath(vigra::triple<CostImageIterator, CostImageIterator, CostAccessor> cost,
                const vigra::Point2D& startingPoint, const vigra::Point2D& endingPoint)
    {
        return minCostPath(cost.first, cost.second, cost.third, startingPoint, endingPoint);
    }
} // namespace enblend


#endif /* PATH_H_INCLUDED_ */

// Local Variables:
// mode: c++
// End:
