import pandas as pd 

df = pd.read_excel("ceps.xlsx")

df.to_csv("ceps.csv", index=False)
print("Conversão concluída com sucesso!")