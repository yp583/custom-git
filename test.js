function greetUser(name) {
    console.log(`Hello ${name}!`);
    const isActive = true;
    return { name, isActive };
}

const user = greetUser("JavaScript");