import axios from "axios"

axios.defaults.httpVersion = 1;
axios.defaults.decompress = true;

async function getCSRF() {
    let response = await axios.get("/csrf", { withCredentials:true});
    axios.defaults.headers["X-CSRF-TOKEN"] = response.data;
    return response.data;
}

async function login(username, password, csrf) {
    let formdata = new URLSearchParams();
    formdata.append("username", username);
    formdata.append("password", password);
    formdata.append("csrf", csrf);

    let response = await axios.post("/login", formdata,{ withCredentials:true});
    return response.data;
}
async function signUp(username, password, csrf) {
    let formdata = new URLSearchParams();
    formdata.append("username", username);
    formdata.append("password", password);
    formdata.append("csrf", csrf);

    let response = await axios.post("/post", formdata,{ withCredentials:true});
    return response.data;
}
