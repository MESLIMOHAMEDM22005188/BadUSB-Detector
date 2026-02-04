// ACCURACY: 1.0
// FEATURES: [AvgDelay, Jitter, MinDelay, MaxDelay, AvgHold]

double score(double * input) {
    return 13.057874230961382 + input[0] * -0.02560117806250737 + input[1] * 0.06748312790700689 + input[2] * -0.03555812250683868 + input[3] * -0.02477945173455901 + input[4] * -0.24948690917842148;
}
