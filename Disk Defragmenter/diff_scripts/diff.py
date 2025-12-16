import sys

def match_pct(actual, expected):
    with open(actual, mode='rb') as file:
        actualContent = file.read()
    with open(expected, mode='rb') as file:
        expectedContent = file.read()

    diff_count = 0.0
    for i in range(0, min(len(actualContent), len(expectedContent))):
        if actualContent[i] != expectedContent[i]:
            diff_count += 1
    
    size_diff = abs(len(actualContent) - len(expectedContent))
    
    diff_count += size_diff
    diff_pct = diff_count/len(expectedContent)
    if diff_pct > 1.0:
        print('diff_pct > 1.0, treating as 1.0')
        diff_pct = 1.0
        
    match_pct = 1.0 - diff_pct
    return abs(round(match_pct, 6))

if __name__ == "__main__":
    actual = sys.argv[1]
    expected = sys.argv[2] 
    print('Input: {}\nExpected: {}\nMatch: {}'.format(actual, expected, match_pct(actual, expected)))