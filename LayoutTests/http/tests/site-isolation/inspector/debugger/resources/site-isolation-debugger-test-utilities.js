async function waitForExpressionInTarget(target, expression, maxAttempts = 20) {
    for (let i = 0; i < maxAttempts; i++) {
        let response = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test", returnByValue: true});
        if (response.result.value)
            return response.result.value;
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
    return undefined;
}
