function matchPct(actual: string, expected: string): string {
    const actualContent = Deno.readFileSync(actual);
    const expectedContent = Deno.readFileSync(expected);

    let diffCount = 0;
    for(let i = 0; i < Math.min(actualContent.length, expectedContent.length); i++) {
        if(actualContent[i] !== expectedContent[i]) {
            diffCount++;
        }
    }

    const sizeDiff = Math.abs(actualContent.length - expectedContent.length);
    diffCount += sizeDiff;
    let diffPct = diffCount/expectedContent.length;
    if(diffPct > 1.0) {
        console.log('diffPct > 1.0, treating as 1.0');
        diffPct = 1.0;
    }

    const matchPct = 1.0 - diffPct;
    return Math.abs(matchPct).toFixed(6);
}

const actual = Deno.args[0];
const expected = Deno.args[1];
console.log(`Input: ${actual}
Expected: ${expected}
Match: ${matchPct(actual, expected)}`);