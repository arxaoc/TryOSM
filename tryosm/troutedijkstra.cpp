#include "troutedijkstra.h"
#include <map>

double vectMult(QPointF v1, QPointF v2)
{
    return v1.x() * v2.y() - v1.y() * v2.x();
}

bool linesCrossed(QPointF p11, QPointF p12, QPointF p21, QPointF p22)
{
    double v1 = vectMult(QPointF(p22.x() - p21.x(), p22.y() - p21.y()), QPointF(p11.x() - p21.x(), p11.y() - p21.y()));
    double v2 = vectMult(QPointF(p22.x() - p21.x(), p22.y() - p21.y()), QPointF(p12.x() - p21.x(), p12.y() - p21.y()));
    double v3 = vectMult(QPointF(p12.x() - p11.x(), p12.y() - p11.y()), QPointF(p21.x() - p11.x(), p21.y() - p11.y()));
    double v4 = vectMult(QPointF(p12.x() - p11.x(), p12.y() - p11.y()), QPointF(p22.x() - p11.x(), p22.y() - p11.y()));
    return ((v1 * v2) < 0) && ((v3 * v4) < 0);
}

TRouteDijkstra::TRouteDijkstra(TOSMWidget * Owner) : TOSMWidget::TRoutingEngine::TRoutingEngine(Owner)
{
    for (TOSMWidget::TNWays::Iterator it = Owner->nways.begin(); it != Owner->nways.end(); it++)
    {
        TID preNodeId = BAD_TID;
        TOSMWidget::TNWay * way = &(it.value());
        if (way->isArea())
        {
            size_t n = way->nodes.size() - 1;
            for (size_t i = 0; i <= n; ++i)
            {
                for (size_t j = 0; j <= n; ++j)
                {
                    bool available = false;
                    if (abs(static_cast<long>(i - j)) <= 1) available = true;
                    if (abs(static_cast<long>(n-i)) + abs(static_cast<long>(j)) <= 1) available = true;
                    if (abs(static_cast<long>(n-j)) + abs(static_cast<long>(i)) <= 1) available = true;
                    if (!available)
                    {
                        available = true;
                        for (size_t k = 1; (k <= n); ++k)
                        {
                            if (linesCrossed(owner->nnodes[way->nodes[i]].toPointF(),
                                             owner->nnodes[way->nodes[j]].toPointF(),
                                             owner->nnodes[way->nodes[k]].toPointF(),
                                             owner->nnodes[way->nodes[k-1]].toPointF()
                                            )
                                )
                            {
                                available = false;
                                break;
                            }
                        }
                    }
                    if (available)
                    {
                        nearKnots(way->nodes[i], way->nodes[j], it.key());
                    }
                }
            }
        }
        else
        {
            for (TOSMWidget::TIDList::Iterator it_n = way->nodes.begin(); it_n != way->nodes.end(); it_n++)
            {
                TID nodeId = *it_n;
                if (Owner->nnodes[nodeId].isKnot())
                {
                    if (preNodeId != BAD_TID) nearKnots(preNodeId, nodeId, it.key());
                    knots.insert(nodeId);
                    preNodeId = nodeId;
                }
            }
        }
    }
}

void TRouteDijkstra::nearKnots(TID node1, TID node2, TID wayId)
{
    TID pt1 = node1, pt2 = node2;
    for (int i = 0; i < 2; i++)
    {
        if (!knotsNear.contains(pt1))
        {
            knotsNear.insert(pt1, QList <TKnotNeighbour> ());
        }
        knotsNear[pt1].append(TKnotNeighbour(pt2, wayId));
//        TOSMWidget::TNWay *way = &(owner->nways[wayId]);
//        qDebug() << "wl " << way->getLength(node1, node2);
        pt1 = node2;
        pt2 = node1;
    }
}

TRouteDijkstra::TKnotNeighbour::TKnotNeighbour(TID node, TID Way)
{
    knot = node;
    way = Way;
}

void TRouteDijkstra::initSearch(TID nodeId,
                                TRouteProfile &profile,
                                TDistanceMap &distances,
                                TSortedDistances &D,
                                TID dest)
{
    for (TIDs::Iterator it = knots.begin(); it != knots.end(); it++)
    {
        distances.insert(*it, W_INF);
        owner->nnodes[*it].metrica = W_INF;
    }
    if (owner->nnodes[nodeId].isKnot())
    {
        D.insert(make_pair(0.0 + ((dest != BAD_TID) ? owner->distance(nodeId, dest) : 0.0), nodeId));
        distances.insert(nodeId, 0.0);
    }
    else
    {
        TOSMWidget::TNWay * way = &owner->nways[*owner->nnodes[nodeId].ownedBy.begin()];
        TID knotNear = BAD_TID;
        int k = way->nodes.size();
        for (k = 0; (k < way->nodes.size()) && (way->nodes[k] != nodeId); k++)
        {
            TID nodei = way->nodes[k];
            if (nodei == nodeId) break;
            if (owner->nnodes[nodei].isKnot()) knotNear = nodei;
        }
        if (knotNear != BAD_TID)
        {
//            TWeight w = profile.getWayWeight(way, nodeId, knotNear);
            TWeight w = profile.getWayWeight(way, nodeId, knotNear);
            D.insert(make_pair(w + ((dest != BAD_TID) ? owner->distance(knotNear, dest) : 0.0), knotNear));
            distances.insert(knotNear, w);
        }
        knotNear = BAD_TID;
        for (int i = k + 1; i < way->nodes.size(); i++)
        {
            TID nodeId = way->nodes[i];
            if (owner->nnodes[nodeId].isKnot())
            {
                knotNear = nodeId;
                break;
            }
        }
        if (knotNear != BAD_TID)
        {
            TWeight w = profile.getWayWeight(way, nodeId, knotNear);
            D.insert(make_pair(w + ((dest != BAD_TID) ? owner->distance(knotNear, dest) : 0.0), knotNear));
            distances.insert(knotNear, w);
        }
    }
}

TRouteDijkstra::TID TRouteDijkstra::getNextKnot(TSortedDistances &D, TIDs &U)
{
    TID v = (*D.begin()).second;
    while(U.contains(v))
    {
        v = BAD_TID;
        D.erase(D.begin());
        if (D.empty()) break;
        v = (*D.begin()).second;
    }
    if (v == BAD_TID) return v;
//    if (D.size() == 0) break;
    D.erase(D.begin());
    U.insert(v);
    return v;
}

void TRouteDijkstra::updateDistances(TID node, TDistanceMap &distances, TSortedDistances &D, TOSMWidget::TRouteProfile &profile, bool reverce, TID dest, TID source)
{
    if (knotsNear.contains(node))
    {
//        qDebug() << "NODE: " << node;
        for (QList <TKnotNeighbour>::Iterator it_nb = knotsNear[node].begin(); it_nb != knotsNear[node].end(); it_nb++)
        {
            TKnotNeighbour * nb = &(*it_nb);
            TID u = nb->knot;
//            qDebug() << "neighbour " << u;
//            qDebug() << "old distance " << distances[u];
            TWeight w = profile.getWayWeight(&owner->nways[nb->way], (reverce ? u : node), (reverce ? node : u));
//            qDebug() << "dist from node " << w;
            distances[u] = std::min(distances[u], distances[node] + w);
//            qDebug() << "new dist " << distances[u];
            TWeight metrica = 0.0, distanceU = distances[u];
            if (dest != BAD_TID)
            {
                metrica = owner->distance(u, dest)/profile.getMaxWaySpeed();
                if ((source != BAD_TID) && (distanceU > 0))
                {
                    double mback = owner->distance(u, source)/profile.getMaxWaySpeed();
                    if (mback > 0)
                    {
                        metrica *= distanceU / mback;
                    }
                }
//                if (rand() % 1000 == 1)
//                {
//                    qDebug() << "distance " << distanceU << "; metrica " << metrica;
//                }
            }
            owner->nnodes[u].metrica = distanceU + metrica;
            D.insert(std::make_pair(distanceU + metrica, u));
//            qDebug() << u << ": " << distances[u];
        }
    }
    else
    {
        qDebug() << "!!! no such cached node " << node;
    }


//    for (TIDs::Iterator it = nnodes[v].containedBy.begin(); it != nnodes[v].containedBy.end(); it++)
//    {
//        TID u;
//        TID w = *it;
//        u = nways[*it].getOtherEnd(v);
//        distances[u] = std::min(distances[u], distances[v] + nways[*it].Weight());
//        D.insert(std::make_pair(distances[u], u));
//    }
}

void TRouteDijkstra::buildRoute(TOSMWidget::TRoute &route, TID start, TDistanceMap &distances, TOSMWidget::TRouteProfile &profile, bool reverce)
{
    TID curPoint = start, nextPoint;
    TWeight curWeight = distances[curPoint];
    for (;;)
    {
        TWeight nextWeight = W_INF;
//        qDebug() << "point " << curPoint << " current weight " << curWeight;
        nextPoint = BAD_TID;
        for (QList <TKnotNeighbour>::iterator it = knotsNear[curPoint].begin(); it != knotsNear[curPoint].end(); it++)
        {
            TKnotNeighbour *kn = &(*it);
            TOSMWidget::TNWay *way = &(owner->nways[kn->way]);
            TWeight w = distances[kn->knot], d = profile.getWayWeight(way, (reverce?curPoint:kn->knot), (reverce?kn->knot:curPoint));
//            qDebug() << kn->knot << "  " << w;
            if (nextWeight > (w+d))
            {
                nextWeight = w;
                nextPoint = kn->knot;
            }
        }
        if (nextWeight < curWeight)
        {
            if (reverce)
                route.nodes.append(nextPoint);
            else
                route.nodes.prepend(nextPoint);
        }
        else break;
        curPoint = nextPoint;
        curWeight = nextWeight;
    }

}

TOSMWidget::TRoute TRouteDijkstra::findPath(TID nodeIdFrom, TID nodeIdTo, TRouteProfile & profile)
{
    if (owner->useMetric)
    {
        return findPath_AStar(nodeIdFrom, nodeIdTo, profile);
    }
    else
    {
        return findPath_DDijkstra(nodeIdFrom, nodeIdTo, profile);
    }
}

TOSMWidget::TRoute TRouteDijkstra::findPath_AStar(TID nodeIdFrom, TID nodeIdTo, TRouteProfile & profile)
{
    qDebug() << "routing with A*";
    TDistanceMap distancesFrom, distancesTo;
    TSortedDistances Dfrom, Dto;
    TIDs Ufrom, Uto;
    initSearch(nodeIdFrom, profile, distancesFrom, Dfrom, nodeIdTo);
    initSearch(nodeIdTo, profile, distancesTo, Dto, nodeIdFrom);
    for (TSortedDistances::iterator it = Dto.begin(); it != Dto.end(); it++)
    {
        Uto.insert(it->second);
    }
    TID contactKnot = BAD_TID;
    while ((Dto.size() > 0) && (Dfrom.size() > 0))
    {
        TID v = getNextKnot(Dfrom, Ufrom);
        if (v == BAD_TID) break;
        if (Uto.contains(v))
        {
            contactKnot = v;
            break;
        }
        updateDistances(v, distancesFrom, Dfrom, profile, false, nodeIdTo);
//        v = getNextKnot(Dto, Uto);
//        if (v == BAD_TID) break;
//        if (Ufrom.contains(v))
//        {
//            contactKnot = v;
//            break;
//        }
//        updateDistances(v, distancesTo, Dto, profile, true, nodeIdFrom);
    }
    TOSMWidget::TRoute route;
    if (contactKnot == BAD_TID)
    {
        qDebug() << "No way!";
    }
    else
    {
        qDebug() << "Way! " << distancesFrom[contactKnot] << "; strait way " << owner->distance(nodeIdFrom, nodeIdTo);
        route.nodes.append(contactKnot);
        buildRoute(route, contactKnot, distancesFrom, profile);
        buildRoute(route, contactKnot, distancesTo, profile, true);
    }
    return route;
}

TOSMWidget::TRoute TRouteDijkstra::findPath_DDijkstra(TID nodeIdFrom, TID nodeIdTo, TRouteProfile & profile)
{
    qDebug() << "routing with Double Dijkstra";
    TDistanceMap distancesFrom, distancesTo;
    TSortedDistances Dfrom, Dto;
    TIDs Ufrom, Uto;
    initSearch(nodeIdFrom, profile, distancesFrom, Dfrom);
    initSearch(nodeIdTo, profile, distancesTo, Dto);
    TID contactKnot = BAD_TID;
    while ((Dto.size() > 0) && (Dfrom.size() > 0))
    {
        TID v = getNextKnot(Dfrom, Ufrom);
        if (v == BAD_TID) break;
        if (Uto.contains(v))
        {
            contactKnot = v;
            break;
        }
        updateDistances(v, distancesFrom, Dfrom, profile);
        v = getNextKnot(Dto, Uto);
        if (v == BAD_TID) break;
        if (Ufrom.contains(v))
        {
            contactKnot = v;
            break;
        }
        updateDistances(v, distancesTo, Dto, profile, true);
    }
    TOSMWidget::TRoute route;
    if (contactKnot == BAD_TID)
    {
        qDebug() << "No way!";
    }
    else
    {
        qDebug() << "Way!";
        route.nodes.append(contactKnot);
        buildRoute(route, contactKnot, distancesFrom, profile);
        buildRoute(route, contactKnot, distancesTo, profile, true);
    }
    return route;
}
