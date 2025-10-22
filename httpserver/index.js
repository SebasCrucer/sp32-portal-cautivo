import express from "express";
import fs from "fs";
import cors from "cors";

const app = express();

app.use(cors());
app.use(express.json());

app.post("/", (req, res) => {
    const oldData = JSON.parse(fs.readFileSync("data.json", "utf8"));
    const newData = [...oldData, req.body]

    fs.writeFileSync("data.json", JSON.stringify(newData));
    res.send("Data saved");
});

app.get("/lts", (req, res) => {
    const data = JSON.parse(fs.readFileSync("data.json", "utf8"));

    if (data.length > 0) {
        res.json([data[data.length - 1]]);
    } else {
        res.json([]);
    }
})

app.get("/", (req, res) => {
    const data = JSON.parse(fs.readFileSync("data.json", "utf8"));

    if (data.length > 0) {
        res.json(data);
    } else {
        res.json([]);
    }
})

app.listen(8001, () => {
    console.log("Server is running on port 8001");
});
