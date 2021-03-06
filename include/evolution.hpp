#pragma once
///@file evolution.hpp
///@brief One-package solution to evolve an evo-devo 3D model according to genetic-algorithm rules
/*! @code
 *  struct FitnessTargets{
 *      float heightToBaseRatio;
 *      float occupancy;
 *  };
 *  
 *  struct FitnessWeights{
 *      float heightToBaseRatioFactor;
 *      float heightToBaseRatioSigma;
 *      float occupancyFactor;
 *      float occupancySigma;
 *  
 *      inline bool operator==(const FitnessWeights &rhs){
 *          return this->heightToBaseRatioFactor==rhs.heightToBaseRatioFactor && this->occupancyFactor==rhs.occupancyFactor && this->heightToBaseRatioSigma==rhs.heightToBaseRatioSigma && this->occupancySigma==rhs.occupancySigma;
 *      }
 *      inline bool operator!=(const FitnessWeights &rhs){
 *          return !(*this==rhs);
 *      }
 *  
 *      FitnessWeights& operator+=(const FitnessWeights &rhs){
 *          this->heightToBaseRatioFactor+=rhs.heightToBaseRatioFactor;
 *          this->occupancyFactor+=rhs.occupancyFactor;
 *          this->heightToBaseRatioSigma+=rhs.heightToBaseRatioSigma;
 *          this->occupancySigma+=rhs.occupancySigma;
 *          return *this;
 *      }
 *      friend FitnessWeights operator+(FitnessWeights lhs, const FitnessWeights &rhs){
 *          lhs+=rhs;
 *          return lhs;
 *      }
 *  
 *      FitnessWeights& operator*=(const int &rhs){
 *          this->heightToBaseRatioFactor*=rhs;
 *          this->occupancyFactor*=rhs;
 *          this->heightToBaseRatioSigma*=rhs;
 *          this->occupancySigma*=rhs;
 *          return *this;
 *      }
 *      friend FitnessWeights operator*(FitnessWeights lhs, const int &rhs){
 *          lhs*=rhs;
 *          return lhs;
 *      }
 *  };
 *  
 *  float fitnessFunction(Body *body, const FitnessTargets &targets, const FitnessWeights &weights){
 *      int minX = 0;
 *      int maxX = 0;
 *      int minY = 0;
 *      int maxY = 0;
 *      int minZ = 0;
 *      int maxZ = 0;
 *      for(uint64_t i=0;i<body->cellsNumber;++i){
 *          minX = body->cells[i].indices[0] < minX ? body->cells[i].indices[0] : minX;
 *          maxX = body->cells[i].indices[0] > maxX ? body->cells[i].indices[0] : maxX;
 *          minY = body->cells[i].indices[1] < minY ? body->cells[i].indices[1] : minY;
 *          maxY = body->cells[i].indices[1] > maxY ? body->cells[i].indices[1] : maxY;
 *          minZ = body->cells[i].indices[2] < minZ ? body->cells[i].indices[2] : minZ;
 *          maxZ = body->cells[i].indices[2] > maxZ ? body->cells[i].indices[2] : maxZ;
 *      }
 *      //occupancy is calculated as the number of cells divided by the number of spaces in the bounding box (0 is a valid position, so we need to add 1 to all deltas)
 *      float occupancy = body->cellsNumber/ float((maxX - minX + 1) * (maxY - minY + 1) * (maxZ - minZ + 1));
 *      float heightToBaseRatio = (maxZ - minZ + 1) / hypotf(maxX - minX + 1, maxY - minY + 1);
 *      //The individual fitnesses are gaussian function, normalizing them wouldn't affect the maximization process so it's avoided as a waste of time
 *      float oFitness      = weights.occupancyFactor * std::exp(-0.5*std::pow((occupancy - targets.occupancy)/weights.occupancySigma, 2));
 *      float htbrFitness   = weights.heightToBaseRatioFactor * std::exp(-0.5*std::pow((heightToBaseRatio - targets.heightToBaseRatio)/weights.heightToBaseRatioSigma, 2));
 *      return oFitness + htbrFitness;
 *  }
 *  int main(){
 *      SelectionStage<FitnessWeights> stages[stagesNumber];
 *      FitnessWeights weights[2] = {{0.1, 3., 0.1, 1.}, {0.01, -0.02, 0.01, -0.005}};
 *      stages[0].weights[0] = weights[0];
 *      stages[0].weights[1] = weights[1];
 *      stages[0].substages.emplace_back(SelectionSubstage::TOURNAMENT, 10, 96);
 *      stages[0].substages.emplace_back(SelectionSubstage::TWO_POINTS_CO, 0.1f, 16);
 *      stages[0].substages.emplace_back(SelectionSubstage::MUTATE, 0.1f, 16);
 *      stages[0].repeats = 10;
 *
 *      FitnessTargets targets{1.5, 0.5};
 *      SelectionPlan<FitnessWeights, FitnessTargets> plan{stages, stagesNumber, true , targets};
 *      plan.stages = stages;
 *      Body        *bodies     = new Body      [populationSize];
 *      for(int i=0;i<populationSize;++i){
 *          bodies[i].cells = new Cell[256*256*256];
 *      }
 *      float       *fitness    = new float     [populationSize];
 *      Genome_t    *genomes    = new Genome_t  [populationSize];
 *      evolve(genomes, populationSize, developmentalStages, bodies, fitness, plan, fitnessFunction, geneticDistance);
 *      for(int i=0;i<populationSize;++i){
 *          delete[] bodies[i].cells;
 *      }
 *      delete[] bodies;
 *      delete[] fitness;
 *      delete[] genomes;
 *      }    
 *      @endcode
 */


#include <vector>
#include <genetic-algorithm.hpp>
#include <evo-devo-gpu.hpp>
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <random>
#include <iostream>

///Contains all the necessary data to call a genetic algorithm function
struct SelectionSubstage{
    ///One of the genetic algorithm functions
    enum Type{
        ROULETTE        = 0b000,
        LINEAR          = 0b001,
        EXPONENTIAL     = 0b010,
        TOURNAMENT      = 0b011,
        TWO_POINTS_CO   = 0b100,
        UNIFORM_CO      = 0b101,
        MUTATE          = 0b110
    } type;
    ///Data specific to each genetic algorithm function
    union{
        float selectionPressure;
        float k1;
        int tournamentSize;
        float mutationProbability;
        float desiredGeneticDistance;
    } param;
    ///The number of individuals that this substage is to generate
    int individuals;
    SelectionSubstage(Type a, int b, int c){type = a; param.tournamentSize = b; individuals=c;}
    SelectionSubstage(Type a, float b, int c){type = a; param.k1= b; individuals=c;}
};

///Defines an entire stage of selection, to go from one generation to the next
template<class W>
struct SelectionStage{
    ///The substages that compose this stage
    ///@note The sum of the individuals member of each substage should be equal to your population size
    std::vector<SelectionSubstage> substages;
    ///weights[0] are the starting weights for each quantity defined in SelectionPlan.targets , while weights[1] is added to weights[0] each repeat
    W weights[2];
    ///The number of times to repeat this SelectionStage
    int repeats;
};

///Defines an entire selection plan
template<class W, class T>
struct SelectionPlan{
    ///Individual stages that compose the plan
    SelectionStage<W> *stages;
    ///Number of stages
    int number;
    ///Whether the fitnesses are to be maximized or minimized. As this has to stay coherent throughout the entire plan, it's defined here
    bool maximizeFitness;
    ///The ideal values for the quantities that the fitness function will calculate
    T targets;
};

/*!
 * @brief   Executes an entire selection plan on a population
 * The end-user should define quantities they want to calculate for each Body, their ideal values and how much each should weight. This information is then put into a user-defined fitness function, and SelectionPlan and SelectionStage have to be specialized using those two as the types for targets (T) and weights (W). Finally, initialize a Genome_t array with generateGenome from evo-devo-gpu and you can call this function.
 * @param[out]  genomes             This has to contain the initial genomes values, as generate by generateGenome, and will hold the final values when the function returns
 * @param[in]   populationSize      The length of the genomes array
 * @param[in]   developmentStages   How many turns each birthBody call will take
 * @param[out]  bodies              This will contain the last generation produced
 * @param[out]  fitness             This will contain the fitness of the last generation produced
 * @param[in]   plan                The selection plan the function will use
 * @param[in]   fitnessFunction     The fitness function evolve() will call to calculate fitnesses
 * @param[in]   geneticDistance     A function that describes the similarity between two genomes
 */
template <class W, class T>
void evolve(Genome_t *genomes, int populationSize, int developmentStages, Body *bodies, float *fitness, SelectionPlan<W, T> plan, float fitnessFunction(Body *body, const T &targets, const W &weights), uint64_t geneticDistance(const Genome_t& first, const Genome_t& second)=myGeneticDistance){
    OpenGLHandles handles;
    if(!initializeOpenGLHandles(&handles)){
        exit(EXIT_FAILURE);    
    }
    Genome_t *thisGenome    = genomes;
    Genome_t *nextGenome    = new Genome_t[populationSize];
    uint8_t **thisGen       = new uint8_t*[populationSize];
    for(int i=0;i<populationSize;++i){
        thisGen[i] = new uint8_t[256*256*256];
    }
    uint8_t **nextGen       = new uint8_t*[populationSize];
    for(int i=0;i<populationSize;++i){
        nextGen[i] = new uint8_t[256*256*256];
    }
    float *currentFitness  = fitness;
    float *nextFitness     = new float[populationSize];
    Body dummyBody;
    dummyBody.cells = new Cell[256*256*256];
    int winners[populationSize];
    std::vector<int> invalidatedBodies;
    invalidatedBodies.reserve(populationSize);
    for(int i=0;i<populationSize;++i){
        std::cout<<"\rDeveloping genome "<<i+1<<" of "<<populationSize<<"..."<<std::flush;
        generateGenome(thisGenome + i);
        loadGenome(&handles, thisGenome + i);
        developBody(&handles, developmentStages);
        birthBody(thisGen[i]);
        isolateBody(&dummyBody, thisGen[i]);
        currentFitness[i] = fitnessFunction(&dummyBody, plan.targets, plan.stages[0].weights[0]);
    }
    std::cout<<std::endl;
    std::random_device rd;  
    std::mt19937 gen(rd()); 
    uint64_t genesLoci[stemCellsTypes*fieldsNumber*8 + 7*fieldsNumber + 1];
    for(int i=0;i<stemCellsTypes*fieldsNumber*8 + 1;++i){
        genesLoci[i] = i;
    }
    for(int i=1;i<7*fieldsNumber+1;++i){
        genesLoci[stemCellsTypes*fieldsNumber*8 + i] = stemCellsTypes*fieldsNumber*8 +2*i;
    }

    for(int i=0;i<plan.number;++i){
        W previousWeights = plan.stages[i].weights[0];
        for(int j=0;j<plan.stages[i].repeats;++j){
            std::cout<<"Stage "<<i+1<<" of "<<plan.number<<", repeat "<<j+1<<" of "<<plan.stages[i].repeats<<std::endl;
            int individualsGenerated = 0;
            for(auto &substage: plan.stages[i].substages){
                switch(substage.type){
                    case SelectionSubstage::ROULETTE:{
                        rouletteRanking(populationSize, currentFitness, winners+individualsGenerated, substage.individuals);
                        break;
                    }
                    case SelectionSubstage::LINEAR:{
                        linearRanking(populationSize, currentFitness, plan.maximizeFitness, substage.param.selectionPressure, winners+individualsGenerated, substage.individuals);
                        break;
                    }
                    case SelectionSubstage::EXPONENTIAL:{
                        exponentialRanking(populationSize, currentFitness, plan.maximizeFitness, substage.param.k1, winners+individualsGenerated, substage.individuals);
                        break;
                    }
                    case SelectionSubstage::TOURNAMENT:{
                        tournamentRanking(populationSize, currentFitness, plan.maximizeFitness, substage.param.tournamentSize, winners+individualsGenerated, substage.individuals);
                        break;
                    }
                    case SelectionSubstage::TWO_POINTS_CO:{
                        //We select half of the parents according to their fitness with a roulette wheel, 
                        //then they select their partners among the rest of the population according to genetic similarity, aiming for substage.param.desiredGeneticDistance
                        //NOTE the distance is not absolute, but relative to the max distance in the population
                        int parents[substage.individuals];
                        rouletteRanking(populationSize, currentFitness, parents, substage.individuals);
                        uint64_t maxDelta = 0;
                        for(int l=0;l<substage.individuals;++l){
                            for(int m=0;m<populationSize;++m){
                                bool ok = true;
                                for(int o=0;o<substage.individuals;++o){
                                    if(m==parents[o]){
                                        ok = false;
                                        break;
                                    }
                                }
                                if(ok){
                                    uint64_t delta = geneticDistance(thisGenome[parents[l]], thisGenome[m]);
                                    if(delta > maxDelta){
                                        maxDelta = delta;
                                    }
                                }
                            }
                        }
                        for(int l=0;l<substage.individuals;++l){
                            float bestDelta = FLT_MAX;
                            int bestMatchIndex = -1;
                            for(int m=0;m<populationSize;++m){
                                bool ok = true;
                                for(int o=0;o<substage.individuals;++o){
                                    if(m==parents[o]){
                                        ok = false;
                                        break;
                                    }
                                }
                                if(ok){
                                    float delta = substage.param.desiredGeneticDistance - geneticDistance(thisGenome[parents[l]], thisGenome[m])/float(maxDelta);
                                    if(delta < bestDelta){
                                        bestDelta = delta;
                                        bestMatchIndex = m;
                                        if(bestDelta==0){
                                            break;
                                        }
                                    }
                                }
                            }
                            twoPointsCrossover((uint8_t*)(thisGenome + parents[l]), (uint8_t*)(thisGenome + bestMatchIndex), sizeof(Genome_t), (uint8_t*) (nextGenome + individualsGenerated + l), genesLoci, sizeof(genesLoci)/sizeof(genesLoci[0]));
                            invalidatedBodies.push_back(individualsGenerated + l);
                        }
                        break;
                    }
                    case SelectionSubstage::UNIFORM_CO:{
                        //We select half of the parents according to their fitness with a roulette wheel, 
                        //then they select their partners among the rest of the population according to genetic similarity, aiming for substage.param.desiredGeneticDistance
                        int parents[substage.individuals];
                        rouletteRanking(populationSize, currentFitness, parents, substage.individuals);
                        uint64_t maxDelta = 0;
                        for(int l=0;l<substage.individuals;++l){
                            for(int m=0;m<populationSize;++m){
                                bool ok = true;
                                for(int o=0;o<substage.individuals;++o){
                                    if(m==parents[o]){
                                        ok = false;
                                        break;
                                    }
                                }
                                if(ok){
                                    uint64_t delta = geneticDistance(thisGenome[parents[l]], thisGenome[m]);
                                    if(delta > maxDelta){
                                        maxDelta = delta;
                                    }
                                }
                            }
                        }
                        for(int l=0;l<substage.individuals;++l){
                            float bestDelta = FLT_MAX;
                            int bestMatchIndex = -1;
                            for(int m=0;m<populationSize;++m){
                                bool ok = true;
                                for(int o=0;o<substage.individuals;++o){
                                    if(m==parents[o]){
                                        ok = false;
                                        break;
                                    }
                                }
                                if(ok){
                                    float delta = substage.param.desiredGeneticDistance - geneticDistance(thisGenome[parents[l]], thisGenome[m])/float(maxDelta);
                                    if(delta < bestDelta){
                                        bestDelta = delta;
                                        bestMatchIndex = m;
                                        if(bestDelta==0){
                                            break;
                                        }
                                    }
                                }
                            }
                            uniformCrossover((uint8_t*)(thisGenome + parents[l]), (uint8_t*)(thisGenome + bestMatchIndex), sizeof(Genome_t), (uint8_t*) (nextGenome + individualsGenerated + l), genesLoci, sizeof(genesLoci)/sizeof(genesLoci[0]));
                            invalidatedBodies.push_back(individualsGenerated + l);
                        }
                        break;
                    }
                    case SelectionSubstage::MUTATE:{
                        //Inside-out Fisher-Yattes Shuffle to get a random permutation of [0, populationSize-1]
                        int permutation[populationSize];
                        for(int l=0;l<populationSize;++l){
                            std::uniform_int_distribution<> dis(0, l);
                            int index = dis(gen);
                            if(index!=l){
                                permutation[l] = permutation[index];
                            }
                            permutation[index] = l;
                        }
                        for(int l=0;l<substage.individuals;++l){
                            nextGenome[individualsGenerated + l] = thisGenome[permutation[l]];
                            mutateGenome(nextGenome + l, substage.param.mutationProbability);
                            invalidatedBodies.push_back(individualsGenerated + l);
                        }
                        break;
                    }
                }
                if(!(substage.type&SelectionSubstage::TWO_POINTS_CO)){
                    for(int l=0;l<substage.individuals;++l){
                        nextGenome[individualsGenerated + l] = thisGenome[winners[l]];
                    }
                    for(int l=0;l<substage.individuals;++l){
                        std::memcpy(nextGen[individualsGenerated+l],  thisGen[winners[l]], 246*256*256);
                    }
                    for(int l=0;l<substage.individuals;++l){
                        nextFitness[individualsGenerated + l] = currentFitness[winners[l]];
                    }
                }
                individualsGenerated+=substage.individuals;
            }
            uint8_t **dummy = nextGen;
            nextGen = thisGen;
            thisGen = dummy;
            float *fitnessDummy = nextFitness;
            nextFitness = currentFitness;
            currentFitness = fitnessDummy;
            Genome_t *genomeDummy = nextGenome;
            nextGenome = thisGenome;
            thisGenome = genomeDummy;
            std::sort(invalidatedBodies.begin(), invalidatedBodies.end());
            for(uint64_t k=0;k<invalidatedBodies.size();++k){
                std::cout<<"\rDeveloping genome "<<k+1<<" of "<<invalidatedBodies.size()<<std::flush;
                loadGenome(&handles, thisGenome + invalidatedBodies[k]);
                developBody(&handles, developmentStages);
                birthBody(thisGen[invalidatedBodies[k]]);
            }
            std::cout<<std::endl;
            if(plan.stages[i].weights[0] + plan.stages[i].weights[1]*j==previousWeights){
                    for(int k : invalidatedBodies){
                        isolateBody(&dummyBody, thisGen[k]);
                        currentFitness[k] = fitnessFunction(&dummyBody, plan.targets, plan.stages[i].weights[0] + plan.stages[i].weights[1] * j);
                    }
            } else {
                    for(int k=0;k<populationSize;++k){
                        isolateBody(&dummyBody, thisGen[k]);
                        currentFitness[k] = fitnessFunction(&dummyBody, plan.targets, plan.stages[i].weights[0] + plan.stages[i].weights[1] * j);
                    }
            }
            invalidatedBodies.clear();
            previousWeights = plan.stages[i].weights[0] + plan.stages[i].weights[1]*j;
        }
    }
    delete[] dummyBody.cells;
    if(genomes!=thisGenome){
        std::memcpy(genomes, thisGenome, populationSize*sizeof(Genome_t));
        delete[] thisGenome;
    } else{
        delete[] nextGenome;
    }
    for(int i=0;i<populationSize;++i){
        isolateBody(bodies + i, thisGen[i]);
    }
    for(int i=0;i<populationSize;++i){
        delete[] thisGen[i];
    }
    delete[] thisGen;
    for(int i=0;i<populationSize;++i){
        delete[] nextGen[i];
    }
    delete[] nextGen;
    if(fitness!=currentFitness){
        std::memcpy(fitness, currentFitness, populationSize*sizeof(float));
        delete[] currentFitness;
    } else{
        delete[] nextFitness;
    }
    deleteHandles(&handles);
}
