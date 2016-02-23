#include <functional>
#include <queue>
#include <vector>

#include "node3d.h"

// CONSTANT VALUES
// possible directions
const int Node3D::dir = 8;
// possible movements
const int Node3D::dx[] = { 1,  1,  0,  -1,  -1, -1,   0,    1 };
const int Node3D::dy[] = { 0,  1,  1,   1,   0, -1,  -1,   -1 };
const float Node3D::dt[] = { 0, 45, 90, 135, 180, 225, 270, 315 };

//###################################################
//                                      MOVEMENT COST
//###################################################
float Node3D::movementCost(const Node3D& pred) const {
  bool penalty = false;
  float distance, tPenalty = 0;

  if (penalty) {
    //heading penalty
    if (abs(t - pred.getT()) > 180) { tPenalty = (360 - abs(t - pred.getT())) / 45; }
    else { tPenalty = abs(t - pred.getT()) / 45; }
    }

  // euclidean distance
  distance = sqrt((x - pred.x) * (x - pred.x) + (y - pred.y) * (y - pred.y));
  return distance + tPenalty;
  }

//###################################################
//                                         COST TO GO
//###################################################
float Node3D::costToGo(const Node3D& goal,
                       const nav_msgs::OccupancyGrid::ConstPtr& oGrid,
                       float costGoal[]) const {
  bool dubins = false;
  bool twoD = false;
  float dubinsCost = 0;
  float euclideanCost = 0;

  // if dubins heuristic is activated calculate the shortest path, constrained
  if (dubins) {
    //start
    double q0[] = { x, y, t / 180 * M_PI };
    // goal
    double q1[] = { goal.x, goal.y, goal.t / 180 * M_PI };
    // turning radius
    float r = 1.5;
    DubinsPath path;
    dubins_init(q0, q1, r, &path);
    dubinsCost = dubins_path_length(&path);
    }

  // if twoD heuristic is activated determine shortest path, unconstrained
  if (twoD && costGoal[y * oGrid->info.width + x] == 0) {
    Node2D start2d(x, y, 0, 0, nullptr);
    Node2D goal2d(goal.x, goal.y, 0, 0, nullptr);
    costGoal[y * oGrid->info.width + x] = Node2D::aStar(start2d, goal2d, oGrid);
    }

  // else calculate the euclidean distance
  euclideanCost = sqrt((x - goal.x) * (x - goal.x) + (y - goal.y) * (y - goal.y));
  // return the maximum of the heuristics, making the heuristic admissable
  return std::max(euclideanCost, std::max(dubinsCost, costGoal[y * oGrid->info.width + x]));
  }

//###################################################
//                                 3D NODE COMPARISON
//###################################################
struct CompareNodes : public
  std::binary_function<Node3D*, Node3D*, bool> {
  bool operator()(const Node3D* lhs, const Node3D* rhs) const {
    return lhs->getC() > rhs->getC();
    }
  };

bool operator == (const Node3D& lhs, const Node3D& rhs) {
  return lhs.getX() == rhs.getX() && lhs.getY() == rhs.getY() &&
         std::abs(std::abs(lhs.getT()) - std::abs(rhs.getT())) <= 22.5;
  //return lhs.getX() == rhs.getX() && lhs.getY() == rhs.getY();
  }

//###################################################
//                                 				3D A*
//###################################################
Node3D* Node3D::aStar(Node3D& start, const Node3D& goal,
                      const nav_msgs::OccupancyGrid::ConstPtr& oGrid, int width, int height, int depth, int length,
                      bool* open, bool* closed, float* cost, float* costToGo, float* costGoal) {
  // MOTION PRIMITIVES
  int dX[8][3];
  int dY[8][3];
  int dT[360];

  // generate motion primitives
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 3; j++) {
      if (i + j < 8) {
        dX[i][j] = Node3D::dx[i + j];
        dY[i][j] = Node3D::dy[i + j];
        }
      else {
        dX[i][j] = Node3D::dx[i + j - 8];
        dY[i][j] = Node3D::dy[i + j - 8];
        }
      }
    }

  // generate theta mapping
  for (int i = 0; i < 360; i++) {
    int factor = (int) i / 45 - 1;

    if (factor < 0) { dT[i] = 7; }
    else { dT[i] = factor; }
    }

  // PREDECESSOR AND SUCCESSOR POSITION
  int x, y, t, xSucc, ySucc, tSucc;
  int idx = 0;
  int idxSucc = 0;

  // OPEN LIST
  std::priority_queue<Node3D*, std::vector<Node3D*>, CompareNodes> O;
  // update g value
  start.updateG(start);
  // update h value
  start.updateH(goal, oGrid, costGoal);
  // push on priority queue
  O.push(&start);
  // add node to open list with total estimated cost
  cost[dT[(int) start.getT()]*width * height + start.getY() * width + start.getX()] = start.getG();

  // continue until O empty
  while (!O.empty()) {
    // create new node pointer
    Node3D* nPred;
    // pop node with lowest cost from priority queue
    nPred = O.top();
    x = nPred->getX();
    y = nPred->getY();
    t = nPred->getT();
    idx = dT[(int) t] * width * height + y * width + x;

    // lazy deletion of rewired node
    if (closed[idx] == true) {
      O.pop();
      continue;
      }
    else if (closed[idx] == false) {
      // remove node from open list
      O.pop();
      open[idx] = false;
      // add node to closed list
      closed[idx] = true;

      // goal test
      if (*nPred == goal) {
        return nPred;
        }
      // continue with search
      else {
        // determine index of motion primitive
        int j = dT[t];

        // create positions of successor nodes
        for (int i = 0; i < 3; i++) {
          xSucc = x + dX[j][i];
          ySucc = y + dY[j][i];

          if (i == 0)  { tSucc = t - 45; }
          else if (i == 1)  { tSucc = t ; }
          else if (i == 2)  { tSucc = t + 45; }

          if (tSucc > 359)  { tSucc = tSucc % 360; }
          else if (tSucc < 0)  { tSucc = 360 + tSucc; }

          idxSucc = dT[(int) tSucc] * width * height + ySucc * width + xSucc;

          // ensure successor is on grid ROW MAJOR^2
          if (xSucc >= 0 && xSucc < width && ySucc >= 0 && ySucc < height && dT[tSucc] >= 0 &&
              dT[tSucc] < depth) {
            // ensure successor is not blocked by obstacle  && obstacleBloating(xSucc, ySucc)
            if (oGrid->data[ySucc * width + xSucc] == 0) {
              // ensure successor is not on closed list
              if (closed[idxSucc] == false) {
                Node3D* nSucc;
                nSucc = new Node3D(xSucc, ySucc, tSucc, nPred->getG(), 0,
                                   nullptr);
                // calculate new g value
                float newG = nPred->getG() + nSucc->movementCost(*nPred);

                // if successor not on open list or g value lower than before put it on open list
                if (open[idxSucc] == false || newG < cost[idxSucc]) {
                  // set predecessor
                  nSucc->setPred(nPred);
                  nSucc->updateG(*nPred);
                  cost[idxSucc] = nSucc->getG();
                  nSucc->updateH(goal, oGrid, costGoal);
                  costToGo[idxSucc] = nSucc->getH();
                  // put successor on open list
                  open[idxSucc] = true;
                  O.push(nSucc);
                  }
                else { delete nSucc; }
                }
              }
            }
          }
        }
      }
    }

  // return 0
  if (O.empty()) {
    return nullptr;
    }
  }
