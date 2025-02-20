#include <stdio.h>
#include "signalMachineUtils.h"

#define REPORT_ADJUSTMENTS FALSE

// Borrowed from Bob Stout http://stjarnhimlen.se/snippets/strrev.c
char *signalUtils_stringReverse(char *str) {
    char *p1, *p2;

    if (! str || ! *str)
        return str;
    for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2) {
        *p1 ^= *p2;
        *p2 ^= *p1;
        *p1 ^= *p2;
    }
    return str;
}

char *signalUtils_getSubSequence(char *seq, int64_t start, int64_t end, bool strand) {
    if (strand) {
        seq = stString_getSubString(seq, start, end - start);
        return seq;
    }
    seq = stString_getSubString(seq, end, start - end);
    return seq;
}

static inline void referenceSequence_loadReference(ReferenceSequence *self,
                                                   char *forwardReferencePath, char *backwardReferencePath) {
    if ((!stFile_exists(forwardReferencePath)) || (!stFile_exists(backwardReferencePath))) {
        st_errAbort("Couldn't find forward or backward reference files, %s, %s",
                    forwardReferencePath, backwardReferencePath);
    }

    self->reference = stFile_getLineFromFile(fopen(forwardReferencePath, "r"));
    self->complementOfReference = stFile_getLineFromFile(fopen(backwardReferencePath, "r"));
}

struct PairwiseAlignment *referenceSequence_copyPairwiseAlignment(struct PairwiseAlignment *pA) {
    struct PairwiseAlignment *A = constructPairwiseAlignment(pA->contig1, pA->start1, pA->end1, pA->strand1,
                                                              pA->contig2, pA->start2, pA->end2, pA->strand2,
                                                              pA->score, NULL);
    return A;
}

static inline void referenceSequence_setTrimmedSeqeuences(ReferenceSequence *self) {
    self->trimmedForwardSequence = signalUtils_getSubSequence(self->reference, self->A->start1,
                                                              self->A->end1, self->A->strand1);
    self->trimmedBackwardSequence = signalUtils_stringReverse(signalUtils_getSubSequence(self->complementOfReference,
                                                                                         self->A->start1, self->A->end1,
                                                                                         self->A->strand1));

}

static inline void referenceSequence_reset(ReferenceSequence *self) {
    free(self->reference);
    free(self->complementOfReference);
    free(self->trimmedForwardSequence);
    free(self->trimmedBackwardSequence);
    self->initialized = FALSE;
}

char *referenceSequence_getTemplateTarget(ReferenceSequence *self) {
    return self->A->strand1 ? self->trimmedForwardSequence : self->trimmedBackwardSequence;
}

char *referenceSequence_getComplementTarget(ReferenceSequence *self) {
    return self->A->strand1 ? self->trimmedBackwardSequence : self->trimmedForwardSequence;
}

ReferenceSequence *signalUtils_ReferenceSequenceConstructFull(char *forwardReferencePath, char *backwardReferencePath,
                                                              struct PairwiseAlignment *pA) {
    ReferenceSequence *R = st_malloc(sizeof(ReferenceSequence));
    referenceSequence_loadReference(R, forwardReferencePath, backwardReferencePath);

    R->A = referenceSequence_copyPairwiseAlignment(pA);

    referenceSequence_setTrimmedSeqeuences(R);

    R->getTemplateTargetSequence = referenceSequence_getTemplateTarget;
    R->getComplementTargetSequence = referenceSequence_getComplementTarget;

    R->initialized = TRUE;

    return R;
}

ReferenceSequence *signalUtils_ReferenceSequenceConstructEmpty(struct PairwiseAlignment *pA) {
    ReferenceSequence *R = st_malloc(sizeof(ReferenceSequence));
    R->A = referenceSequence_copyPairwiseAlignment(pA);

    R->reference = NULL;
    R->complementOfReference = NULL;
    R->trimmedForwardSequence = NULL;
    R->trimmedBackwardSequence = NULL;

    R->getTemplateTargetSequence = referenceSequence_getTemplateTarget;
    R->getComplementTargetSequence = referenceSequence_getComplementTarget;

    R->initialized = FALSE;

    return R;
}

void signalUtils_ReferenceSequenceSet(ReferenceSequence *self,
                                      char *forwardReferencePath, char *backwardReferencePath) {
    //if (self->initialized) {
    //    referenceSequence_reset(self);
    //}

    referenceSequence_loadReference(self, forwardReferencePath, backwardReferencePath);
    referenceSequence_setTrimmedSeqeuences(self);

    self->initialized = TRUE;
}

void signalUtils_ReferenceSequenceDestruct(ReferenceSequence *self) {
    //destructPairwiseAlignment(self->A);
    free(self->A->contig1);
    free(self->A->contig2);
    free(self->A);
    free(self->reference);
    free(self->complementOfReference);
    free(self->trimmedForwardSequence);
    free(self->trimmedBackwardSequence);
    free(self);
}

static void signalUtils_rebasePairwiseAlignmentCoordinates(int64_t *start, int64_t *end, int64_t *strand,
                                                           int64_t coordinateShift, bool flipStrand) {
    *start += coordinateShift;
    *end += coordinateShift;
    if (flipStrand) {
        *strand = *strand ? 0 : 1;
        int64_t i = *end;
        *end = *start;
        *start = i;
    }
}

stList *signalUtils_guideAlignmentToRebasedAnchorPairs(struct PairwiseAlignment *pA, PairwiseAlignmentParameters *p) {
    // check if we need to flip the reference
    bool flipStrand1 = !pA->strand1;
    int64_t refCoordShift = (pA->strand1 ? pA->start1 : pA->end1);

    // rebase the reference alignment to (0), but not the nanopore read, this is corrected when remapping the
    // anchorPairs
    signalUtils_rebasePairwiseAlignmentCoordinates(&(pA->start1), &(pA->end1), &(pA->strand1), -refCoordShift,
                                                   flipStrand1);
    checkPairwiseAlignment(pA);

    //Convert input alignment into anchor pairs
    stList *unfilteredAnchorPairs = convertPairwiseForwardStrandAlignmentToAnchorPairs(
            pA, p->constraintDiagonalTrim);

    // sort
    stList_sort(unfilteredAnchorPairs, (int (*)(const void *, const void *)) stIntTuple_cmpFn);

    // filter
    stList *anchorPairs = filterToRemoveOverlap(unfilteredAnchorPairs);

    return anchorPairs;
}

stList *signalUtils_getRemappedAnchorPairs(stList *unmappedAnchors, int64_t *eventMap, int64_t mapOffset) {
    stList *remapedAnchors = nanopore_remapAnchorPairsWithOffset(unmappedAnchors, eventMap, mapOffset);
    stList *filteredRemappedAnchors = filterToRemoveOverlap(remapedAnchors);
    return filteredRemappedAnchors;
}

stList *signalUtils_templateOneDAssignmentsFromRead(NanoporeRead *npRead, StateMachine *sM, double ignore) {
    (void) ignore;
    return nanopore_getOneDAssignmentsFromRead(npRead->templateStrandEventMap, npRead->templateEvents,
                                               npRead->templateRead, npRead->templateReadLength,
                                               sM->alphabet, sM->alphabetSize, sM->kmerLength);
}

stList *signalUtils_complementOneDAssignmentsFromRead(NanoporeRead *npRead, StateMachine *sM, double ignore) {
    (void) ignore;
    return nanopore_getOneDAssignmentsFromRead(npRead->complementStrandEventMap, npRead->complementEvents,
                                               npRead->complementRead, npRead->complementReadLength,
                                               sM->alphabet, sM->alphabetSize, sM->kmerLength);
}

void signalUtils_estimateNanoporeParams(StateMachine *sM, NanoporeRead *npRead,
                                        NanoporeReadAdjustmentParameters *params, double assignmentThreshold,
                                        stList *(*assignmentFunction)(NanoporeRead *, StateMachine *, double),
                                        void (*driftAdjustmentFunction)(NanoporeRead *)) {
    if (REPORT_ADJUSTMENTS) {
        st_uglyf("SENTINEL - Re-estimating parameters\n");
        st_uglyf("SENTINEL - Before: scale: %f shift: %f var: %f drift %f\n", params->scale, params->shift, params->var,
                 params->drift);
        st_uglyf("SENTINEL - Before: scale_sd: %f var_sd: %f\n", params->scale_sd, params->var_sd);
    }
    stList *map = assignmentFunction(npRead, sM, assignmentThreshold);
    if (REPORT_ADJUSTMENTS) {
        st_uglyf("SENTINEL - Map is %lld long\n", stList_length(map));
    }

    nanopore_compute_mean_scale_params(sM->EMISSION_MATCH_MATRIX, map, params, TRUE, TRUE);
    nanopore_compute_noise_scale_params(sM->EMISSION_MATCH_MATRIX, map, params);

    if (REPORT_ADJUSTMENTS) {
        st_uglyf("SENTINEL - After: scale: %f shift: %f var: %f drift: %f\n", params->scale, params->shift, params->var,
                 params->drift);
        st_uglyf("SENTINEL - After: scale_sd: %f var_sd: %f\n", params->scale_sd, params->var_sd);
    }

    sM->scale = params->scale;
    sM->shift = params->shift;
    sM->var = params->var;

//    printNanoporeReadAdjustmentParameters(params);

    if ((sM->scale != params->scale) || (sM->shift != params->shift) || (sM->var != params->var)) {
        st_errAbort("ERROR - Problem updating stateMachine\n");
    }

    driftAdjustmentFunction(npRead);
    emissions_signal_scaleNoise(sM, *params);

    stList_destruct(map);
    return;
}

void signalUtils_estimateNanoporeParamsFromTable(const char *modelPath,
                                                 NanoporeRead *npRead, NanoporeReadAdjustmentParameters *params,
                                                 double assignmentThreshold,
                                                 stList *(*assignmentFunction)(NanoporeRead *, StateMachine *, double),
                                                 void (*driftAdjustmentFunction)(NanoporeRead *)) {
    if (!stFile_exists(modelPath)) {
        st_errAbort("signalUtils_estimateNanoporeParamsFromTable: didn't find table, looked here %s\n", modelPath);
    }
    StateMachine *sM = getStateMachine3_descaled(modelPath, *params, FALSE);
    signalUtils_estimateNanoporeParams(sM, npRead, params, assignmentThreshold,
                                       assignmentFunction, driftAdjustmentFunction);
    stateMachine_destruct(sM);
    return;
}
