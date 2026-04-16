const puppeteer = require('puppeteer');

(async () => {
    const browser = await puppeteer.launch();
    const page = await browser.newPage();
    await page.setViewport({width: 1494, height: 799});
    await page.goto('http://localhost:8080/');
    
    // Wait for the simulation to render (3 seconds)
    await new Promise(r => setTimeout(r, 4000));
    
    // Screenshot
    await page.screenshot({path: 'C:\\Users\\Nandi\\.gemini\\antigravity\\brain\\f72a3342-0175-4c75-b271-cef80e778ba0\\puppeteer_check.png'});
    await browser.close();
})();
