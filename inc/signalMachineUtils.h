#ifndef SIGNAL_H_
#define SIGNAL_H_

#include "pairwiseAlignment.h"
#include "pairwiseAligner.h"

typedef struct _referenceSequence ReferenceSequence;
struct _referenceSequence {
    char *reference;
    char *complementOfReference;

    char *trimmedForwardSequence;
    char *trimmedBackwardSequence;

    struct PairwiseAlignment *A;  // pairwise alignment

    char *(*getTemplateTargetSequence)(ReferenceSequence *R);
    char *(*getComplementTargetSequence)(ReferenceSequence *R);

    bool initialized;
};

char *signalUtils_stringReverse(char *str);

char *referenceSequence_getComplementTarget(ReferenceSequence *self);

char *referenceSequence_getTemplateTarget(ReferenceSequence *self);

struct PairwiseAlignment *referenceSequence_copyPairwiseAlignment(struct PairwiseAlignment *pA);

ReferenceSequence *signalUtils_ReferenceSequenceConstructFull(char *forwardReferencePath, char *backwardReferencePath,
                                                              struct PairwiseAlignment *pA);

ReferenceSequence *signalUtils_ReferenceSequenceConstructEmpty(struct PairwiseAlignment *pA);

void signalUtils_ReferenceSequenceSet(ReferenceSequence *self, char *forwardReferencePath, char *backwardReferencePath);

char *signalUtils_getSubSequence(char *seq, int64_t start, int64_t end, bool strand);

//void referenceSequence_loadReference(ReferenceSequence *R, char *forwardReferencePath, char *backwardReferencePath);

stList *signalUtils_guideAlignmentToRebasedAnchorPairs(struct PairwiseAlignment *pA, PairwiseAlignmentParameters *p);

stList *signalUtils_getRemappedAnchorPairs(stList *unmappedAnchors, int64_t *eventMap, int64_t mapOffset);

void signalUtils_estimateNanoporeParams(StateMachine *sM, NanoporeRead *npRead,
                                        NanoporeReadAdjustmentParameters *params, double assignmentThreshold,
                                        stList *(*assignmentFunction)(NanoporeRead *, StateMachine *, double),
                                        void (*driftAdjustmentFunction)(NanoporeRead *));

void signalUtils_estimateNanoporeParamsFromTable(const char *modelPath,
                                                 NanoporeRead *npRead, NanoporeReadAdjustmentParameters *params,
                                                 double assignmentThreshold,
                                                 stList *(*assignmentFunction)(NanoporeRead *, StateMachine *, double),
                                                 void (*driftAdjustmentFunction)(NanoporeRead *));

stList *signalUtils_templateOneDAssignmentsFromRead(NanoporeRead *npRead, StateMachine *sM, double ignore);

stList *signalUtils_complementOneDAssignmentsFromRead(NanoporeRead *npRead, StateMachine *sM, double ignore);

void signalUtils_ReferenceSequenceDestruct(ReferenceSequence *self);

void printFoo();

#endif