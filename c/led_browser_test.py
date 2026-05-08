from selenium import webdriver
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import Select
import time

URL = 'http://192.168.11.1:8080/led'

opt = Options()
opt.add_argument('--no-sandbox')
opt.add_experimental_option('detach', True)  # keep window open after script ends

driver = webdriver.Chrome(options=opt)
driver.get(URL)
time.sleep(1)

tests = [
    ('All On',           2, 2, 2),
    ('All Off',          1, 1, 1),
    ('Only Wan On',      2, 1, 1),
    ('Only Lan On',      1, 2, 1),
    ('Only Wifi On',     1, 1, 2),
    ('Wan Slow Flash',   3, 1, 1),
    ('Wan Fast Flash',   4, 1, 1),
    ('Mixed',            2, 3, 4),
    ('All Keep',        15,15,15),
]

for name, wan, lan, wifi in tests:
    print(f'\n=== {name} ===')
    print(f'Setting wan={wan} lan={lan} wifi={wifi}')

    Select(driver.find_element(By.ID, 'wan')).select_by_value(str(wan))
    Select(driver.find_element(By.ID, 'lan')).select_by_value(str(lan))
    Select(driver.find_element(By.ID, 'wifi')).select_by_value(str(wifi))

    btn = driver.find_element(By.XPATH, "//button[text()='Set LEDs']")
    driver.execute_script('arguments[0].click()', btn)
    time.sleep(1.5)

    cur = driver.find_element(By.ID, 'cur').text
    st  = driver.find_element(By.ID, 'st').text
    print(f'  Current: {cur}')
    print(f'  Result:  {st}')

print('\nDone. Chrome window stays open.')
