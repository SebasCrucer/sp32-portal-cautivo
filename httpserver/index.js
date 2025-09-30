import express from "express";
import fs from "fs";

const app = express();
app.use(express.json());

app.post("/", (req, res) => {
    const oldData = JSON.parse(fs.readFileSync("data.json", "utf8"));
    const newData = [...oldData, req.body]

    fs.writeFileSync("data.json", JSON.stringify(newData));
    res.send("Data saved");
});

app.listen(8001, () => {
    console.log("Server is running on port 3000");
});